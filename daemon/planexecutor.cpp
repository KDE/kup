// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "planexecutor.h"
#include "bupjob.h"
#include "buprepairjob.h"
#include "bupverificationjob.h"
#include "kupdaemon.h"
#include "kupdaemon_debug.h"
#include "rsyncjob.h"

#include <KFormat>
#include <KIO/DirectorySizeJob>
#include <KIO/OpenUrlJob>
#include <KLocalizedString>
#include <KNotification>
#include <QDBusConnection>
#include <QDBusReply>
#include <QDir>
#include <QStorageInfo>
#include <QTimer>

static const QString cPwrMgmtServiceName = QStringLiteral("org.freedesktop.PowerManagement");
static const QString cPwrMgmtPath = QStringLiteral("/org/freedesktop/PowerManagement");
static const QString cPwrMgmtInhibitInterface = QStringLiteral("org.freedesktop.PowerManagement.Inhibit");
static const QString cPwrMgmtInterface = QStringLiteral("org.freedesktop.PowerManagement");

PlanExecutor::PlanExecutor(BackupPlan *pPlan, KupDaemon *pKupDaemon)
    : QObject(pKupDaemon)
    , mState(NOT_AVAILABLE)
    , mPlan(pPlan)
    , mQuestion(nullptr)
    , mFailNotification(nullptr)
    , mIntegrityNotification(nullptr)
    , mRepairNotification(nullptr)
    , mLastState(NOT_AVAILABLE)
    , mKupDaemon(pKupDaemon)
    , mSleepCookie(0)
{
    QString lCachePath = QString::fromLocal8Bit(qgetenv("XDG_CACHE_HOME").constData());
    if (lCachePath.isEmpty()) {
        lCachePath = QDir::homePath();
        lCachePath.append(QStringLiteral("/.cache"));
    }
    lCachePath.append(QStringLiteral("/kup"));
    QDir lCacheDir(lCachePath);
    if (!lCacheDir.exists()) {
        if (!lCacheDir.mkpath(lCachePath)) {
            lCachePath = QStringLiteral("/tmp");
        }
    }
    mLogFilePath = lCachePath;
    mLogFilePath.append(QStringLiteral("/kup_plan"));
    mLogFilePath.append(QString::number(mPlan->planNumber()));
    mLogFilePath.append(QStringLiteral(".log"));

    mSchedulingTimer = new QTimer(this);
    mSchedulingTimer->setSingleShot(true);
    connect(mSchedulingTimer, SIGNAL(timeout()), SLOT(enterAvailableState()));
}

PlanExecutor::~PlanExecutor() = default;

QString PlanExecutor::currentActivityTitle()
{
    switch (mState) {
    case BACKUP_RUNNING:
        return i18nc("status in tooltip", "Saving backup");
    case INTEGRITY_TESTING:
        return i18nc("status in tooltip", "Checking backup integrity");
    case REPAIRING:
        return i18nc("status in tooltip", "Repairing backups");
    default:;
    }

    switch (mPlan->backupStatus()) {
    case BackupPlan::GOOD:
        return i18nc("status in tooltip", "Backup status OK");
    case BackupPlan::MEDIUM:
        return i18nc("status in tooltip", "New backup suggested");
    case BackupPlan::BAD:
        return i18nc("status in tooltip", "New backup needed");
    default:;
    }
    return QString();
}

// dispatcher code for entering one of the available states
void PlanExecutor::enterAvailableState()
{
    if (mState == NOT_AVAILABLE) {
        mState = WAITING_FOR_FIRST_BACKUP; // initial child state of "Available" state
        emit stateChanged();
    }
    QDateTime lNow = QDateTime::currentDateTimeUtc();
    switch (mPlan->mScheduleType) {
    case BackupPlan::MANUAL:
        break;
    case BackupPlan::INTERVAL: {
        QDateTime lNextTime = mPlan->nextScheduledTime();
        if (!lNextTime.isValid() || lNextTime < lNow) {
            if (!mPlan->mLastCompleteBackup.isValid())
                askUserOrStart(xi18nc("@info", "Do you want to save a first backup now?"));
            else {
                QString t = KFormat().formatSpelloutDuration(static_cast<quint64>(mPlan->mLastCompleteBackup.secsTo(lNow)) * 1000);
                askUserOrStart(xi18nc("@info",
                                      "It has been %1 since last backup was saved.\n"
                                      "Save a new backup now?",
                                      t));
            }
        } else {
            // Call this method again in five minutes to check if it's time.
            // The alternative of sleeping all the way to when backup saving is due
            // has the problem that time spent suspended is not counted.
            mSchedulingTimer->start(5 * 60 * 1000);
        }
        break;
    }
    case BackupPlan::USAGE:
        if (!mPlan->mLastCompleteBackup.isValid()) {
            askUserOrStart(xi18nc("@info", "Do you want to save a first backup now?"));
        } else if (mPlan->mAccumulatedUsageTime > static_cast<quint32>(mPlan->mUsageLimit) * 3600) {
            QString t = KFormat().formatSpelloutDuration(mPlan->mAccumulatedUsageTime * 1000);
            askUserOrStart(xi18nc("@info",
                                  "You have been active for %1 since last backup was saved.\n"
                                  "Save a new backup now?",
                                  t));
        }
        break;
    }
}

void PlanExecutor::askUserOrStart(const QString &pUserQuestion)
{
    // Only ask the first time after destination has become available.
    // Always ask if power saving is active.
    if ((mPlan->mAskBeforeTakingBackup && mState == WAITING_FOR_FIRST_BACKUP) || powerSaveActive()) {
        askUser(pUserQuestion);
    } else {
        startBackupSaveJob();
    }
}

void PlanExecutor::enterNotAvailableState()
{
    discardUserQuestion();
    mSchedulingTimer->stop();
    mState = NOT_AVAILABLE;
    emit stateChanged();
}

void PlanExecutor::askUser(const QString &pQuestion)
{
    discardUserQuestion();
    mQuestion = new KNotification(QStringLiteral("StartBackup"), KNotification::Persistent);
    mQuestion->setTitle(mPlan->mDescription);
    mQuestion->setText(pQuestion);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QStringList lAnswers;
    lAnswers << xi18nc("@action:button", "Yes") << xi18nc("@action:button", "No");
    mQuestion->setActions(lAnswers);
    connect(mQuestion, SIGNAL(action1Activated()), SLOT(startBackupSaveJob()));
    connect(mQuestion, SIGNAL(action2Activated()), SLOT(discardUserQuestion()));
#else
    KNotificationAction *yes = mQuestion->addAction(xi18nc("@action:button", "Yes"));
    connect(yes, &KNotificationAction::activated, this, &PlanExecutor::startBackupSaveJob);

    KNotificationAction *no = mQuestion->addAction(xi18nc("@action:button", "No"));
    connect(no, &KNotificationAction::activated, this, &PlanExecutor::discardUserQuestion);
#endif
    connect(mQuestion, SIGNAL(closed()), SLOT(discardUserQuestion()));
    connect(mQuestion, SIGNAL(ignored()), SLOT(discardUserQuestion()));
    // enter this "do nothing" state, if user answers "no" or ignores, remain there
    mState = WAITING_FOR_MANUAL_BACKUP;
    emit stateChanged();
    mQuestion->sendEvent();
}

void PlanExecutor::discardUserQuestion()
{
    if (mQuestion) {
        mQuestion->deleteLater();
        mQuestion = nullptr;
    }
}

void PlanExecutor::notifyBackupFailed(KJob *pFailedJob)
{
    discardFailNotification();
    mFailNotification = new KNotification(QStringLiteral("BackupFailed"), KNotification::Persistent);
    mFailNotification->setTitle(xi18nc("@title:window", "Saving of Backup Failed"));
    mFailNotification->setText(pFailedJob->errorText());

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QStringList lAnswers;
    if (pFailedJob->error() == BackupJob::ErrorWithLog) {
        lAnswers << xi18nc("@action:button", "Show log file");
        connect(mFailNotification, SIGNAL(action1Activated()), SLOT(showLog()));
    } else if (pFailedJob->error() == BackupJob::ErrorSuggestRepair) {
        lAnswers << xi18nc("@action:button", "Yes");
        lAnswers << xi18nc("@action:button", "No");
        connect(mFailNotification, SIGNAL(action1Activated()), SLOT(startRepairJob()));
    } else if (pFailedJob->error() == BackupJob::ErrorSourcesConfig) {
        lAnswers << xi18nc("@action:button", "Open settings");
        connect(mFailNotification, &KNotification::action1Activated, this, [this] {
            QProcess::startDetached(QStringLiteral("kcmshell5"),
                                    {QStringLiteral("--args"), QStringLiteral("show_sources %1").arg(mPlan->planNumber()), QStringLiteral("kcm_kup")});
        });
    }
    mFailNotification->setActions(lAnswers);

    connect(mFailNotification, SIGNAL(action2Activated()), SLOT(discardFailNotification()));
#else
    if (pFailedJob->error() == BackupJob::ErrorWithLog) {
        KNotificationAction *showLogFile = mFailNotification->addAction(xi18nc("@action:button", "Show log file"));
        connect(showLogFile, &KNotificationAction::activated, this, &PlanExecutor::showLog);
    } else if (pFailedJob->error() == BackupJob::ErrorSuggestRepair) {
        KNotificationAction *yes = mFailNotification->addAction(xi18nc("@action:button", "Yes"));
        connect(yes, &KNotificationAction::activated, this, &PlanExecutor::startRepairJob);

        KNotificationAction *no = mFailNotification->addAction(xi18nc("@action:button", "No"));
        connect(no, &KNotificationAction::activated, this, &PlanExecutor::discardFailNotification);
    } else if (pFailedJob->error() == BackupJob::ErrorSourcesConfig) {
        KNotificationAction *openSettings = mFailNotification->addAction(xi18nc("@action:button", "Open settings"));
        connect(openSettings, &KNotificationAction::activated, this, [this] {
            QProcess::startDetached(QStringLiteral("kcmshell5"),
                                    {QStringLiteral("--args"), QStringLiteral("show_sources %1").arg(mPlan->planNumber()), QStringLiteral("kcm_kup")});
        });
    }
#endif
    connect(mFailNotification, SIGNAL(closed()), SLOT(discardFailNotification()));
    connect(mFailNotification, SIGNAL(ignored()), SLOT(discardFailNotification()));
    mFailNotification->sendEvent();
}

void PlanExecutor::discardFailNotification()
{
    if (mFailNotification) {
        mFailNotification->deleteLater();
        mFailNotification = nullptr;
    }
}

void PlanExecutor::notifyBackupSucceeded()
{
    auto *lNotification = new KNotification(QStringLiteral("BackupSucceeded"));
    lNotification->setTitle(xi18nc("@title:window", "Backup Saved"));
    lNotification->setText(xi18nc("@info notification", "Saving backup completed successfully."));
    lNotification->sendEvent();
}

void PlanExecutor::showLog()
{
    auto *job = new KIO::OpenUrlJob(QUrl::fromLocalFile(mLogFilePath), QStringLiteral("text/x-log"));
    job->start();
}

void PlanExecutor::startIntegrityCheck()
{
    if (mPlan->mBackupType != BackupPlan::BupType || busy() || !destinationAvailable()) {
        return;
    }
    KJob *lJob = new BupVerificationJob(*mPlan, mDestinationPath, mLogFilePath, mKupDaemon);
    connect(lJob, &KJob::result, this, &PlanExecutor::integrityCheckFinished);
    lJob->start();
    mLastState = mState;
    mState = INTEGRITY_TESTING;
    emit stateChanged();
    startSleepInhibit();
}

void PlanExecutor::startRepairJob()
{
    if (mPlan->mBackupType != BackupPlan::BupType || busy() || !destinationAvailable()) {
        return;
    }
    KJob *lJob = new BupRepairJob(*mPlan, mDestinationPath, mLogFilePath, mKupDaemon);
    connect(lJob, &KJob::result, this, &PlanExecutor::repairFinished);
    lJob->start();
    mLastState = mState;
    mState = REPAIRING;
    emit stateChanged();
    startSleepInhibit();
}

void PlanExecutor::startBackupSaveJob()
{
    if (busy() || !destinationAvailable()) {
        return;
    }
    discardUserQuestion();
    mState = BACKUP_RUNNING;
    emit stateChanged();
    startSleepInhibit();
    startBackup();
}

void PlanExecutor::startBackup()
{
    QFileInfo lInfo(mDestinationPath);
    if (!lInfo.isWritable()) {
        KNotification::event(KNotification::Error,
                             xi18nc("@title:window", "Problem"),
                             xi18nc("notification", "You don't have write permission to backup destination."));
        exitBackupRunningState(false);
        return;
    }
    BackupJob *lJob = createBackupJob();
    if (lJob == nullptr) {
        KNotification::event(KNotification::Error, xi18nc("@title:window", "Problem"), xi18nc("notification", "Invalid type of backup in configuration."));
        exitBackupRunningState(false);
        return;
    }
    connect(lJob, &KJob::result, this, &PlanExecutor::finishBackup);
    lJob->start();
}

void PlanExecutor::finishBackup(KJob *pJob)
{
    if (pJob->error()) {
        if (pJob->error() != KJob::KilledJobError) {
            notifyBackupFailed(pJob);
        }
        exitBackupRunningState(false);
    } else {
        notifyBackupSucceeded();
        mPlan->mLastCompleteBackup = QDateTime::currentDateTimeUtc();
        QStorageInfo storageInfo(mDestinationPath);
        if (storageInfo.isValid())
            mPlan->mLastAvailableSpace = static_cast<double>(storageInfo.bytesAvailable());
        else
            mPlan->mLastAvailableSpace = -1.0; // unknown size

        auto lSizeJob = KIO::directorySize(QUrl::fromLocalFile(mDestinationPath));
        connect(lSizeJob, &KJob::result, this, &PlanExecutor::finishSizeCheck);
        lSizeJob->start();
    }
}

void PlanExecutor::finishSizeCheck(KJob *pJob)
{
    if (pJob->error()) {
        KNotification::event(KNotification::Error, xi18nc("@title:window", "Problem"), pJob->errorText());
        mPlan->mLastBackupSize = -1.0; // unknown size
    } else {
        auto lSizeJob = qobject_cast<KIO::DirectorySizeJob *>(pJob);
        mPlan->mLastBackupSize = static_cast<double>(lSizeJob->totalSize());
    }
    mPlan->save();
    exitBackupRunningState(pJob->error() == 0);
}

void PlanExecutor::integrityCheckFinished(KJob *pJob)
{
    endSleepInhibit();
    discardIntegrityNotification();
    mIntegrityNotification = new KNotification(QStringLiteral("IntegrityCheckCompleted"), KNotification::Persistent);
    mIntegrityNotification->setTitle(xi18nc("@title:window", "Integrity Check Completed"));
    mIntegrityNotification->setText(pJob->errorText());
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QStringList lAnswers;
    if (pJob->error() == BackupJob::ErrorWithLog) {
        lAnswers << xi18nc("@action:button", "Show log file");
        connect(mIntegrityNotification, SIGNAL(action1Activated()), SLOT(showLog()));
    } else if (pJob->error() == BackupJob::ErrorSuggestRepair) {
        lAnswers << xi18nc("@action:button", "Yes");
        lAnswers << xi18nc("@action:button", "No");
        connect(mIntegrityNotification, SIGNAL(action1Activated()), SLOT(startRepairJob()));
    }
    mIntegrityNotification->setActions(lAnswers);

    connect(mIntegrityNotification, SIGNAL(action2Activated()), SLOT(discardIntegrityNotification()));
#else
    if (pJob->error() == BackupJob::ErrorWithLog) {
        KNotificationAction *showLogFile = new KNotificationAction(xi18nc("@action:button", "Show log file"));
        connect(showLogFile, &KNotificationAction::activated, this, &PlanExecutor::showLog);
    } else if (pJob->error() == BackupJob::ErrorSuggestRepair) {
        KNotificationAction *yes = mIntegrityNotification->addAction(xi18nc("@action:button", "Yes"));
        connect(yes, &KNotificationAction::activated, this, &PlanExecutor::startRepairJob);

        KNotificationAction *no = mIntegrityNotification->addAction(xi18nc("@action:button", "No"));
        connect(no, &KNotificationAction::activated, this, &PlanExecutor::discardIntegrityNotification);
    }
#endif

    connect(mIntegrityNotification, SIGNAL(closed()), SLOT(discardIntegrityNotification()));
    connect(mIntegrityNotification, SIGNAL(ignored()), SLOT(discardIntegrityNotification()));
    mIntegrityNotification->sendEvent();

    if (mState == INTEGRITY_TESTING) { // only restore if nothing has changed during the run
        mState = mLastState;
    }
    emit stateChanged();
}

void PlanExecutor::discardIntegrityNotification()
{
    if (mIntegrityNotification) {
        mIntegrityNotification->deleteLater();
        mIntegrityNotification = nullptr;
    }
}

void PlanExecutor::repairFinished(KJob *pJob)
{
    endSleepInhibit();
    discardRepairNotification();
    mRepairNotification = new KNotification(QStringLiteral("RepairCompleted"), KNotification::Persistent);
    mRepairNotification->setTitle(xi18nc("@title:window", "Repair Completed"));
    mRepairNotification->setText(pJob->errorText());
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QStringList lAnswers;
    lAnswers << xi18nc("@action:button", "Show log file");
    mRepairNotification->setActions(lAnswers);
    connect(mRepairNotification, SIGNAL(action1Activated()), SLOT(showLog()));
#else
    KNotificationAction *showLogFile = mRepairNotification->addAction(xi18nc("@action:button", "Show log file"));
    connect(showLogFile, &KNotificationAction::activated, this, &PlanExecutor::showLog);
#endif
    connect(mRepairNotification, SIGNAL(closed()), SLOT(discardRepairNotification()));
    connect(mRepairNotification, SIGNAL(ignored()), SLOT(discardRepairNotification()));
    mRepairNotification->sendEvent();

    if (mState == REPAIRING) { // only restore if nothing has changed during the run
        mState = mLastState;
    }
    emit stateChanged();
}

void PlanExecutor::discardRepairNotification()
{
    if (mRepairNotification) {
        mRepairNotification->deleteLater();
        mRepairNotification = nullptr;
    }
}

void PlanExecutor::startSleepInhibit()
{
    if (mSleepCookie != 0) {
        return;
    }
    QDBusMessage lMsg = QDBusMessage::createMethodCall(cPwrMgmtServiceName, cPwrMgmtPath, cPwrMgmtInhibitInterface, QStringLiteral("Inhibit"));
    lMsg << i18n("Kup Backup System");
    lMsg << currentActivityTitle();
    QDBusReply<uint> lReply = QDBusConnection::sessionBus().call(lMsg);
    mSleepCookie = lReply.value();
}

void PlanExecutor::endSleepInhibit()
{
    if (mSleepCookie == 0) {
        return;
    }
    QDBusMessage lMsg = QDBusMessage::createMethodCall(cPwrMgmtServiceName, cPwrMgmtPath, cPwrMgmtInhibitInterface, QStringLiteral("UnInhibit"));
    lMsg << mSleepCookie;
    QDBusConnection::sessionBus().asyncCall(lMsg);
    mSleepCookie = 0;
}

void PlanExecutor::exitBackupRunningState(bool pWasSuccessful)
{
    endSleepInhibit();
    if (pWasSuccessful) {
        if (mPlan->mScheduleType == BackupPlan::USAGE) {
            // reset usage time after successful backup
            mPlan->mAccumulatedUsageTime = 0;
            mPlan->save();
        }
        mState = WAITING_FOR_BACKUP_AGAIN;
        emit stateChanged();

        // don't know if status actually changed, potentially did... so trigger a re-read of status
        emit backupStatusChanged();

        // re-enter the main "available" state dispatcher
        enterAvailableState();
    } else {
        mState = WAITING_FOR_MANUAL_BACKUP;
        emit stateChanged();
    }
}

void PlanExecutor::updateAccumulatedUsageTime()
{
    if (mState == BACKUP_RUNNING) { // usage time during backup doesn't count...
        return;
    }

    if (mPlan->mScheduleType == BackupPlan::USAGE) {
        mPlan->mAccumulatedUsageTime += KUP_USAGE_MONITOR_INTERVAL_S;
        mPlan->save();
    }

    // trigger refresh of backup status, potentially changed since some time has passed...
    // this is the reason why this slot is called repeatedly even when
    // not in BackupPlan::USAGE mode
    emit backupStatusChanged();

    // if we're waiting to run backup again, check if it is time now.
    if (mPlan->mScheduleType == BackupPlan::USAGE && (mState == WAITING_FOR_FIRST_BACKUP || mState == WAITING_FOR_BACKUP_AGAIN)) {
        enterAvailableState();
    }
}

void PlanExecutor::showBackupFiles()
{
    if (mState == NOT_AVAILABLE)
        return;
    if (mPlan->mBackupType == BackupPlan::BupType) {
        QStringList lArgs;
        lArgs << mDestinationPath;
        KProcess::startDetached(QStringLiteral("kup-filedigger"), lArgs);
    } else if (mPlan->mBackupType == BackupPlan::RsyncType) {
        auto *job = new KIO::OpenUrlJob(QUrl::fromLocalFile(mDestinationPath));
        job->start();
    }
}

void PlanExecutor::showBackupPurger()
{
    if (mPlan->mBackupType != BackupPlan::BupType || busy() || !destinationAvailable()) {
        return;
    }
    QStringList lArgs;
    lArgs << mDestinationPath;
    KProcess::startDetached(QStringLiteral("kup-purger"), lArgs);
}

BackupJob *PlanExecutor::createBackupJob()
{
    if (mPlan->mBackupType == BackupPlan::BupType) {
        return new BupJob(*mPlan, mDestinationPath, mLogFilePath, mKupDaemon);
    }
    if (mPlan->mBackupType == BackupPlan::RsyncType) {
        return new RsyncJob(*mPlan, mDestinationPath, mLogFilePath, mKupDaemon);
    }
    qCWarning(KUPDAEMON) << "Invalid backup type in configuration!";
    return nullptr;
}

bool PlanExecutor::powerSaveActive()
{
    QDBusMessage lMsg = QDBusMessage::createMethodCall(cPwrMgmtServiceName, cPwrMgmtPath, cPwrMgmtInterface, QStringLiteral("GetPowerSaveStatus"));
    QDBusReply<bool> lReply = QDBusConnection::sessionBus().call(lMsg);
    return lReply.value();
}
