// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef PLANEXECUTOR_H
#define PLANEXECUTOR_H

#include "backupjob.h"
#include "backupplan.h"

#include <KProcess>

class KupDaemon;

class KNotification;
class KProcess;

class QTimer;

// Accumulate usage time every KUP_USAGE_MONITOR_INTERVAL_S while user is active.
// Consider user inactive after KUP_IDLE_TIMEOUT_S s of no keyboard or mouse activity.
#define KUP_USAGE_MONITOR_INTERVAL_S 2 * 60
#define KUP_IDLE_TIMEOUT_S 30

class PlanExecutor : public QObject
{
    Q_OBJECT
public:
    PlanExecutor(BackupPlan *pPlan, KupDaemon *pKupDaemon);
    ~PlanExecutor() override;

    BackupPlan::ScheduleType scheduleType()
    {
        return static_cast<BackupPlan::ScheduleType>(mPlan->mScheduleType);
    }

    bool busy()
    {
        return mState == BACKUP_RUNNING || mState == INTEGRITY_TESTING || mState == REPAIRING;
    }
    bool destinationAvailable()
    {
        return mState != NOT_AVAILABLE;
    }

    QString currentActivityTitle();

    enum ExecutorState {
        NOT_AVAILABLE,
        WAITING_FOR_FIRST_BACKUP,
        WAITING_FOR_BACKUP_AGAIN,
        BACKUP_RUNNING,
        WAITING_FOR_MANUAL_BACKUP,
        INTEGRITY_TESTING,
        REPAIRING
    };
    ExecutorState mState;
    QString mDestinationPath;
    QString mLogFilePath;
    BackupPlan *mPlan;

public slots:
    virtual void checkStatus() = 0;
    virtual void showBackupFiles();
    virtual void showBackupPurger();
    void updateAccumulatedUsageTime();
    void startIntegrityCheck();
    void startRepairJob();
    void startBackupSaveJob();
    void showLog();

signals:
    void stateChanged();
    void backupStatusChanged();

protected slots:
    virtual void startBackup();
    void finishBackup(KJob *pJob);
    void finishSizeCheck(KJob *pJob);

    void exitBackupRunningState(bool pWasSuccessful);
    void enterAvailableState();
    void askUserOrStart(const QString &pUserQuestion);
    void enterNotAvailableState();

    void askUser(const QString &pQuestion);
    void discardUserQuestion();

    void notifyBackupFailed(KJob *pFailedJob);
    void discardFailNotification();

    static void notifyBackupSucceeded();

    void integrityCheckFinished(KJob *pJob);
    void discardIntegrityNotification();
    void repairFinished(KJob *pJob);
    void discardRepairNotification();

    void startSleepInhibit();
    void endSleepInhibit();

protected:
    BackupJob *createBackupJob();
    static bool powerSaveActive();

    KNotification *mQuestion;
    QTimer *mSchedulingTimer;
    KNotification *mFailNotification;
    KNotification *mIntegrityNotification;
    KNotification *mRepairNotification;
    ExecutorState mLastState;
    KupDaemon *mKupDaemon;
    uint mSleepCookie;
};

#endif // PLANEXECUTOR_H
