// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "kupdaemon.h"
#include "backupplan.h"
#include "edexecutor.h"
#include "fsexecutor.h"
#include "kupsettings.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QPushButton>
#include <QSessionManager>
#include <QTimer>

#include <KIdleTime>
#include <KLocalizedString>
#include <KUiServerV2JobTracker>

KupDaemon::KupDaemon()
    : mConfig(KSharedConfig::openConfig(QStringLiteral("kuprc")))
    , mSettings(new KupSettings(mConfig, this))
    , mUsageAccTimer(new QTimer(this))
    , mStatusUpdateTimer(new QTimer(this))
    , mWaitingToReloadConfig(false)
    , mJobTracker(new KUiServerV2JobTracker(this))
{
}

KupDaemon::~KupDaemon()
{
    while (!mExecutors.isEmpty()) {
        delete mExecutors.takeFirst();
    }
    KIdleTime::instance()->removeAllIdleTimeouts();
}

bool KupDaemon::shouldStart()
{
    return mSettings->mBackupsEnabled;
}

void KupDaemon::setupGuiStuff()
{
    // timer to update logged time and also trigger warning if too long
    // time has now passed since last backup
    mUsageAccTimer->setInterval(KUP_USAGE_MONITOR_INTERVAL_S * 1000);
    mUsageAccTimer->start();
    KIdleTime *lIdleTime = KIdleTime::instance();
    lIdleTime->addIdleTimeout(KUP_IDLE_TIMEOUT_S * 1000);
    connect(lIdleTime, qOverload<int, int>(&KIdleTime::timeoutReached), mUsageAccTimer, &QTimer::stop);
    connect(lIdleTime, qOverload<int, int>(&KIdleTime::timeoutReached), lIdleTime, &KIdleTime::catchNextResumeEvent);
    connect(lIdleTime, &KIdleTime::resumingFromIdle, mUsageAccTimer, qOverload<>(&QTimer::start));

    // delay status update to avoid sending a status to plasma applet
    // that will be changed again just a microsecond later anyway
    mStatusUpdateTimer->setInterval(500);
    mStatusUpdateTimer->setSingleShot(true);
    connect(mStatusUpdateTimer, &QTimer::timeout, this, [this] {
        sendPlansChangedSignal();

        if (mWaitingToReloadConfig) {
            // quite likely the config can be reloaded now, give it a try.
            QTimer::singleShot(0, this, SLOT(reloadConfig()));
        }
    });

    QDBusConnection lDBus = QDBusConnection::sessionBus();
    if (lDBus.isConnected()) {
        if (lDBus.registerService(KUP_DBUS_SERVICE_NAME)) {
            lDBus.registerObject(KUP_DBUS_OBJECT_PATH, this, QDBusConnection::ExportAllSlots);
        }
    }

    reloadConfig();
}

void KupDaemon::reloadConfig()
{
    auto lBusy = std::any_of(mExecutors.cbegin(), mExecutors.cend(), [&](auto pExecutor) {
        return pExecutor->busy();
    });

    if (lBusy) {
        mWaitingToReloadConfig = true;
        return;
    }

    mWaitingToReloadConfig = false;

    mSettings->load();
    while (!mExecutors.isEmpty()) {
        delete mExecutors.takeFirst();
    }
    if (!mSettings->mBackupsEnabled)
        qApp->quit();

    setupExecutors();
    // Juuuust in case all those executors for some reason never
    // triggered an updated status... Doesn't hurt anyway.
    mStatusUpdateTimer->start();
}

// This method is exposed over DBus so that filedigger can call it
void KupDaemon::runIntegrityCheck(const QString &pPath)
{
    foreach (PlanExecutor *lExecutor, mExecutors) {
        // if caller passes in an empty path, startsWith will return true and we will try to check
        // all backup plans.
        if (lExecutor->mDestinationPath.startsWith(pPath)) {
            lExecutor->startIntegrityCheck();
        }
    }
}

// This method is exposed over DBus so that user scripts can call it
void KupDaemon::saveNewBackup(int pPlanNumber)
{
    if (pPlanNumber > 0 && pPlanNumber <= mExecutors.count()) {
        mExecutors[pPlanNumber - 1]->startBackupSaveJob();
    }
}

void KupDaemon::browseBackup(int pPlanNumber) const
{
    mExecutors.at(pPlanNumber)->showBackupFiles();
}

void KupDaemon::purgeBackups(int pPlanNumber)
{
    mExecutors.at(pPlanNumber)->showBackupPurger();
}

QVariantList KupDaemon::getPlans() const
{
    QVariantList lPlans;
    for (const auto lPlanExec : std::as_const(mExecutors)) {
        const auto lPlan = lPlanExec->mPlan;
        QVariantMap lPlanInfo;
        lPlanInfo["Description"] = lPlan->mDescription;
        lPlanInfo["Status"] = QVariant::fromValue(lPlan->backupStatus()).toString();
        lPlanInfo["Busy"] = lPlanExec->busy();
        lPlanInfo["ActivityState"] = QVariant::fromValue(lPlanExec->mState).toString();
        lPlanInfo["LogFile"] = lPlanExec->mLogFilePath;
        lPlanInfo["LogFileExists"] = QFileInfo::exists(lPlanExec->mLogFilePath);
        lPlanInfo["Type"] = QVariant::fromValue(static_cast<BackupPlan::BackupType>(lPlan->mBackupType)).toString();
        lPlanInfo["ScheduleType"] = QVariant::fromValue(static_cast<BackupPlan::ScheduleType>(lPlan->mScheduleType)).toString();
        lPlanInfo["DestAvailable"] = lPlanExec->destinationAvailable();
        lPlanInfo["LastCompleteBackup"] = lPlan->mLastCompleteBackup.toUTC();
        lPlanInfo["LastBackupSize"] = lPlan->mLastBackupSize;
        lPlanInfo["LastFreeSpace"] = lPlan->mLastAvailableSpace;
        lPlans << lPlanInfo;
    }
    return lPlans;
}

QString KupDaemon::getRepositoryPath(const QString &pPath) const
{
    for (const auto lExecutor : std::as_const(mExecutors)) {
        auto lPlan = lExecutor->mPlan;

        bool lIsIncluded = std::any_of(lPlan->mPathsIncluded.cbegin(), lPlan->mPathsIncluded.cend(), [&](const QString &lIncludedPath) {
            bool lIsExcluded = std::any_of(lPlan->mPathsExcluded.cbegin(), lPlan->mPathsExcluded.cend(), [&](const QString &lExcludedPath) {
                return pPath.startsWith(lExcludedPath) && lExcludedPath.length() > lIncludedPath.length();
            });
            return pPath.startsWith(lIncludedPath) && !lIsExcluded;
        });

        if (lExecutor->destinationAvailable() && lPlan->mBackupType == BackupPlan::BupType && lIsIncluded) {
            return lExecutor->mDestinationPath;
        }
    }

    return QString();
}

void KupDaemon::registerJob(KJob *pJob)
{
    mJobTracker->registerJob(pJob);
}

void KupDaemon::unregisterJob(KJob *pJob)
{
    mJobTracker->unregisterJob(pJob);
}

void KupDaemon::slotShutdownRequest(QSessionManager &pManager)
{
    // this will make session management not try (and fail because of KDBusService starting only
    // one instance) to start this daemon. We have autostart for the purpose of launching this
    // daemon instead.
    pManager.setRestartHint(QSessionManager::RestartNever);

    auto lExecutor = std::find_if(mExecutors.cbegin(), mExecutors.cend(), [&](auto pExecutor) {
        return pExecutor->busy() && pManager.allowsErrorInteraction();
    });

    if (lExecutor != mExecutors.cend()) {
        QMessageBox lMessageBox;
        const QPushButton *lContinueButton = lMessageBox.addButton(i18n("Continue"), QMessageBox::RejectRole);
        lMessageBox.addButton(i18n("Stop"), QMessageBox::AcceptRole);
        lMessageBox.setText(i18nc("%1 is a text explaining the current activity", "Currently busy: %1", (*lExecutor)->currentActivityTitle()));
        lMessageBox.setInformativeText(i18n("Do you really want to stop?"));
        lMessageBox.setIcon(QMessageBox::Warning);
        lMessageBox.setWindowIcon(QIcon::fromTheme(QStringLiteral("kup")));
        lMessageBox.setWindowTitle(i18n("User Backups"));
        lMessageBox.exec();

        if (lMessageBox.clickedButton() == lContinueButton) {
            pManager.cancel();
        }
    }
}

void KupDaemon::setupExecutors()
{
    for (int i = 0; i < mSettings->mNumberOfPlans; ++i) {
        PlanExecutor *lExecutor;
        auto *lPlan = new BackupPlan(i + 1, mConfig, this);
        if (lPlan->mPathsIncluded.isEmpty()) {
            delete lPlan;
            continue;
        }
        if (lPlan->mDestinationType == 0) {
            lExecutor = new FSExecutor(lPlan, this);
        } else if (lPlan->mDestinationType == 1) {
            lExecutor = new EDExecutor(lPlan, this);
        } else {
            delete lPlan;
            continue;
        }
        connect(lExecutor, &PlanExecutor::stateChanged, this, [this] {
            mStatusUpdateTimer->start();
        });
        connect(lExecutor, &PlanExecutor::backupStatusChanged, this, [this] {
            mStatusUpdateTimer->start();
        });
        connect(mUsageAccTimer, &QTimer::timeout, lExecutor, &PlanExecutor::updateAccumulatedUsageTime);
        lExecutor->checkStatus();
        mExecutors.append(lExecutor);
    }
}

void KupDaemon::sendPlansChangedSignal()
{
    QDBusMessage lSignal = QDBusMessage::createSignal(KUP_DBUS_OBJECT_PATH, KUP_DBUS_SERVICE_NAME, QStringLiteral("PlansChanged"));
    lSignal << getPlans();
    QDBusConnection::sessionBus().send(lSignal);
}
