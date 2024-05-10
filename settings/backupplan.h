// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef BACKUPPLAN_H
#define BACKUPPLAN_H

#include <KCoreConfigSkeleton>

class Schedule;

class BackupPlan : public KCoreConfigSkeleton
{
public:
    BackupPlan(int pPlanNumber, KSharedConfigPtr pConfig, QObject *pParent = nullptr);
    int planNumber() const
    {
        return mPlanNumber;
    }
    virtual void setPlanNumber(int pPlanNumber);
    QString statusText();
    void copyFrom(const BackupPlan &pPlan);

    QString mDescription;
    QStringList mPathsIncluded;
    QStringList mPathsExcluded;
    enum BackupType { BupType = 0, RsyncType };
    qint32 mBackupType{};
    qint32 mBackupVersion{};

    enum ScheduleType { MANUAL = 0, INTERVAL, USAGE };
    qint32 mScheduleType{};
    qint32 mScheduleInterval{};
    qint32 mScheduleIntervalUnit{};
    qint32 mUsageLimit{}; // in hours
    bool mAskBeforeTakingBackup{};

    qint32 mDestinationType{};
    QUrl mFilesystemDestinationPath;
    QString mExternalUUID;
    QString mExternalDestinationPath;
    QString mExternalVolumeLabel;
    QString mExternalDeviceDescription;
    int mExternalPartitionNumber{};
    int mExternalPartitionsOnDrive{};
    qulonglong mExternalVolumeCapacity{};

    bool mShowHiddenFolders{};
    bool mGenerateRecoveryInfo{};
    bool mCheckBackups{};
    bool mExcludePatterns{};
    QString mExcludePatternsPath;

    QDateTime mLastCompleteBackup;
    // Size of the last backup in bytes.
    double mLastBackupSize{};
    // Last known available space on destination
    double mLastAvailableSpace{};
    // How long has Kup been running since last backup (s)
    quint32 mAccumulatedUsageTime{};

    virtual QDateTime nextScheduledTime();
    virtual qint64 scheduleIntervalInSeconds();

    enum Status { GOOD, MEDIUM, BAD, NO_STATUS };
    Status backupStatus();
    static QString iconName(Status pStatus);
    QString absoluteExcludesFilePath();

protected:
    void usrRead() override;
    int mPlanNumber;
};

#endif // BACKUPPLAN_H
