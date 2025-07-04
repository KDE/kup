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
    , mLocalServer(new QLocalServer(this))
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
        foreach (QLocalSocket *lSocket, mSockets) {
            sendStatus(lSocket);
        }

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
    QString lSocketName = QStringLiteral("kup-daemon-");
    lSocketName += QString::fromLocal8Bit(qgetenv("USER"));

    connect(mLocalServer, &QLocalServer::newConnection, this, [this] {
        QLocalSocket *lSocket = mLocalServer->nextPendingConnection();
        if (lSocket == nullptr) {
            return;
        }
        sendStatus(lSocket);
        mSockets.append(lSocket);
        connect(lSocket, &QLocalSocket::readyRead, this, [this, lSocket] {
            handleRequests(lSocket);
        });
        connect(lSocket, &QLocalSocket::disconnected, this, [this, lSocket] {
            mSockets.removeAll(lSocket);
            lSocket->deleteLater();
        });
    });
    // remove old socket first in case it's still there, otherwise listen() fails.
    QLocalServer::removeServer(lSocketName);
    mLocalServer->listen(lSocketName);

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

void KupDaemon::handleRequests(QLocalSocket *pSocket)
{
    if (pSocket->bytesAvailable() <= 0) {
        return;
    }
    QJsonDocument lDoc = QJsonDocument::fromJson(pSocket->readAll());
    if (!lDoc.isObject()) {
        return;
    }
    QJsonObject lCommand = lDoc.object();
    QString lOperation = lCommand["operation name"].toString();
    if (lOperation == QStringLiteral("get status")) {
        sendStatus(pSocket);
        return;
    }
    if (lOperation == QStringLiteral("reload")) {
        reloadConfig();
        return;
    }

    int lPlanNumber = lCommand["plan number"].toInt(-1);
    if (lPlanNumber < 0 || lPlanNumber >= mExecutors.count()) {
        return;
    }
    if (lOperation == QStringLiteral("save backup")) {
        mExecutors.at(lPlanNumber)->startBackupSaveJob();
    }
    if (lOperation == QStringLiteral("remove backups")) {
        mExecutors.at(lPlanNumber)->showBackupPurger();
    }
    if (lOperation == QStringLiteral("show log file")) {
        mExecutors.at(lPlanNumber)->showLog();
    }
    if (lOperation == QStringLiteral("show backup files")) {
        mExecutors.at(lPlanNumber)->showBackupFiles();
    }
}

void KupDaemon::sendStatus(QLocalSocket *pSocket)
{
    bool lTrayIconActive = false;
    bool lAnyPlanBusy = false;
    // If all backup plans have status == NO_STATUS then tooltip title will be empty
    QString lToolTipTitle;
    QString lToolTipSubTitle = i18nc("status in tooltip", "Backup destination not available");
    QString lToolTipIconName = QStringLiteral("kup");

    if (mExecutors.isEmpty()) {
        lToolTipTitle = i18n("No backup plans configured");
        lToolTipSubTitle.clear();
    }

    foreach (PlanExecutor *lExec, mExecutors) {
        if (lExec->destinationAvailable()) {
            lToolTipSubTitle = i18nc("status in tooltip", "Backup destination available");
            if (lExec->scheduleType() == BackupPlan::MANUAL) {
                lTrayIconActive = true;
            }
        }
    }

    foreach (PlanExecutor *lExec, mExecutors) {
        if (lExec->mPlan->backupStatus() == BackupPlan::GOOD) {
            lToolTipIconName = BackupPlan::iconName(BackupPlan::GOOD);
            lToolTipTitle = i18nc("status in tooltip", "Backup status OK");
        }
    }

    foreach (PlanExecutor *lExec, mExecutors) {
        if (lExec->mPlan->backupStatus() == BackupPlan::MEDIUM) {
            lToolTipIconName = BackupPlan::iconName(BackupPlan::MEDIUM);
            lToolTipTitle = i18nc("status in tooltip", "New backup suggested");
        }
    }

    foreach (PlanExecutor *lExec, mExecutors) {
        if (lExec->mPlan->backupStatus() == BackupPlan::BAD) {
            lToolTipIconName = BackupPlan::iconName(BackupPlan::BAD);
            lToolTipTitle = i18nc("status in tooltip", "New backup needed");
            lTrayIconActive = true;
        }
    }

    foreach (PlanExecutor *lExecutor, mExecutors) {
        if (lExecutor->busy()) {
            lToolTipIconName = QStringLiteral("kup");
            lToolTipTitle = lExecutor->currentActivityTitle();
            lToolTipSubTitle = lExecutor->mPlan->mDescription;
            lAnyPlanBusy = true;
        }
    }

    if (lToolTipTitle.isEmpty() && !lToolTipSubTitle.isEmpty()) {
        lToolTipTitle = lToolTipSubTitle;
        lToolTipSubTitle.clear();
    }

    QJsonObject lStatus;
    lStatus["event"] = QStringLiteral("status update");
    lStatus["tray icon active"] = lTrayIconActive;
    lStatus["tooltip icon name"] = lToolTipIconName;
    lStatus["tooltip title"] = lToolTipTitle;
    lStatus["tooltip subtitle"] = lToolTipSubTitle;
    lStatus["any plan busy"] = lAnyPlanBusy;
    lStatus["no plan reason"] = mExecutors.isEmpty() ? i18n("No backup plans configured") : QString();
    QJsonArray lPlans;
    foreach (PlanExecutor *lExecutor, mExecutors) {
        QJsonObject lPlan;
        lPlan[QStringLiteral("description")] = lExecutor->mPlan->mDescription;
        lPlan[QStringLiteral("destination available")] = lExecutor->destinationAvailable();
        lPlan[QStringLiteral("status heading")] = lExecutor->currentActivityTitle();
        lPlan[QStringLiteral("status details")] = lExecutor->mPlan->statusText();
        lPlan[QStringLiteral("icon name")] = BackupPlan::iconName(lExecutor->mPlan->backupStatus());
        lPlan[QStringLiteral("log file exists")] = QFileInfo::exists(lExecutor->mLogFilePath);
        lPlan[QStringLiteral("busy")] = lExecutor->busy();
        lPlan[QStringLiteral("bup type")] = lExecutor->mPlan->mBackupType == BackupPlan::BupType;
        lPlans.append(lPlan);
    }
    lStatus["plans"] = lPlans;
    QJsonDocument lDoc(lStatus);
    pSocket->write(lDoc.toJson());
}
