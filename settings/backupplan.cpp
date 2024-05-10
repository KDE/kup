// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "backupplan.h"
#include "kuputils.h"

#include <QDir>
#include <QStandardPaths>
#include <QString>

#include <KFormat>
#include <KLocalizedString>
#include <utility>

BackupPlan::BackupPlan(int pPlanNumber, KSharedConfigPtr pConfig, QObject *pParent)
    : KCoreConfigSkeleton(std::move(pConfig), pParent)
    , mPlanNumber(pPlanNumber)
{
    setCurrentGroup(QString(QStringLiteral("Plan/%1")).arg(mPlanNumber));

    addItemString(QStringLiteral("Description"),
                  mDescription,
                  xi18nc("@label Default name for a new backup plan, %1 is the number of the plan in order", "Backup plan %1", pPlanNumber));
    QStringList lDefaultIncludeList;
    lDefaultIncludeList << QDir::homePath();
    addItemStringList(QStringLiteral("Paths included"), mPathsIncluded, lDefaultIncludeList);
    QStringList lDefaultExcludeList;
    lDefaultExcludeList << QDir::homePath() + QStringLiteral("/.cache");
    lDefaultExcludeList << QDir::homePath() + QStringLiteral("/.bup");
    lDefaultExcludeList << QDir::homePath() + QStringLiteral("/.thumbnails");
    lDefaultExcludeList << QDir::homePath() + QStringLiteral("/.local/share/Trash");
    lDefaultExcludeList << QDir::homePath() + QStringLiteral("/.local/share/baloo");
    lDefaultExcludeList << QDir::homePath() + QStringLiteral("/.local/share/TelegramDesktop/tdata/temp");
    lDefaultExcludeList << QDir::homePath() + QStringLiteral("/.config/Riot/Cache");
    QMutableStringListIterator i(lDefaultExcludeList);
    while (i.hasNext()) {
        ensureNoTrailingSlash(i.next());
    }

    addItemStringList(QStringLiteral("Paths excluded"), mPathsExcluded, lDefaultExcludeList);
    addItemInt(QStringLiteral("Backup type"), mBackupType);
    addItemInt(QStringLiteral("Backup version"), mBackupVersion);

    addItemInt(QStringLiteral("Schedule type"), mScheduleType, 2);
    addItemInt(QStringLiteral("Schedule interval"), mScheduleInterval, 1);
    addItemInt(QStringLiteral("Schedule interval unit"), mScheduleIntervalUnit, 3);
    addItemInt(QStringLiteral("Usage limit"), mUsageLimit, 25);
    addItemBool(QStringLiteral("Ask first"), mAskBeforeTakingBackup, true);

    addItemInt(QStringLiteral("Destination type"), mDestinationType, 1);
    addItem(new KCoreConfigSkeleton::ItemUrl(currentGroup(),
                                             QStringLiteral("Filesystem destination path"),
                                             mFilesystemDestinationPath,
                                             QUrl::fromLocalFile(QDir::homePath() + QStringLiteral("/.bup"))));
    addItemString(QStringLiteral("External drive UUID"), mExternalUUID);
    addItemPath(QStringLiteral("External drive destination path"), mExternalDestinationPath, i18n("Backups"));
    addItemString(QStringLiteral("External volume label"), mExternalVolumeLabel);
    addItemULongLong(QStringLiteral("External volume capacity"), mExternalVolumeCapacity);
    addItemString(QStringLiteral("External device description"), mExternalDeviceDescription);
    addItemInt(QStringLiteral("External partition number"), mExternalPartitionNumber);
    addItemInt(QStringLiteral("External partitions count"), mExternalPartitionsOnDrive);

    addItemBool(QStringLiteral("Show hidden folders"), mShowHiddenFolders);
    addItemBool(QStringLiteral("Generate recovery info"), mGenerateRecoveryInfo);
    addItemBool(QStringLiteral("Check backups"), mCheckBackups);
    addItemBool(QStringLiteral("Exclude patterns"), mExcludePatterns);
    addItemString(QStringLiteral("Exclude patterns file path"), mExcludePatternsPath);

    addItemDateTime(QStringLiteral("Last complete backup"), mLastCompleteBackup);
    addItemDouble(QStringLiteral("Last backup size"), mLastBackupSize);
    addItemDouble(QStringLiteral("Last available space"), mLastAvailableSpace);
    addItemUInt(QStringLiteral("Accumulated usage time"), mAccumulatedUsageTime);
    load();
}

void BackupPlan::setPlanNumber(int pPlanNumber)
{
    mPlanNumber = pPlanNumber;
    QString lGroupName = QString(QStringLiteral("Plan/%1")).arg(mPlanNumber);
    foreach (KConfigSkeletonItem *lItem, items()) {
        lItem->setGroup(lGroupName);
    }
}

void BackupPlan::copyFrom(const BackupPlan &pPlan)
{
    mDescription = i18nc("default description of newly duplicated backup plan", "%1 (copy)", pPlan.mDescription);
    mPathsIncluded = pPlan.mPathsIncluded;
    mPathsExcluded = pPlan.mPathsExcluded;
    mBackupType = pPlan.mBackupType;
    mScheduleType = pPlan.mScheduleType;
    mScheduleInterval = pPlan.mScheduleInterval;
    mScheduleIntervalUnit = pPlan.mScheduleIntervalUnit;
    mUsageLimit = pPlan.mUsageLimit;
    mAskBeforeTakingBackup = pPlan.mAskBeforeTakingBackup;
    mDestinationType = pPlan.mDestinationType;
    mFilesystemDestinationPath = pPlan.mFilesystemDestinationPath;
    mExternalUUID = pPlan.mExternalUUID;
    mExternalDestinationPath = pPlan.mExternalDestinationPath;
    mExternalVolumeLabel = pPlan.mExternalVolumeLabel;
    mExternalDeviceDescription = pPlan.mExternalDeviceDescription;
    mExternalPartitionNumber = pPlan.mExternalPartitionNumber;
    mExternalPartitionsOnDrive = pPlan.mExternalPartitionsOnDrive;
    mExternalVolumeCapacity = pPlan.mExternalVolumeCapacity;
    mShowHiddenFolders = pPlan.mShowHiddenFolders;
    mGenerateRecoveryInfo = pPlan.mGenerateRecoveryInfo;
    mCheckBackups = pPlan.mCheckBackups;
}

QDateTime BackupPlan::nextScheduledTime()
{
    Q_ASSERT(mScheduleType == 1);
    if (!mLastCompleteBackup.isValid())
        return QDateTime(); // plan has never been run

    return mLastCompleteBackup.addSecs(scheduleIntervalInSeconds());
}

qint64 BackupPlan::scheduleIntervalInSeconds()
{
    Q_ASSERT(mScheduleType == 1);

    switch (mScheduleIntervalUnit) {
    case 0:
        return mScheduleInterval * 60;
    case 1:
        return mScheduleInterval * 60 * 60;
    case 2:
        return mScheduleInterval * 60 * 60 * 24;
    case 3:
        return mScheduleInterval * 60 * 60 * 24 * 7;
    default:
        return 0;
    }
}

BackupPlan::Status BackupPlan::backupStatus()
{
    if (!mLastCompleteBackup.isValid()) {
        return BAD;
    }
    if (mScheduleType == MANUAL) {
        return NO_STATUS;
    }

    qint64 lStatus = 5; // trigger BAD status if schedule type is something strange
    qint64 lInterval = 1;

    switch (mScheduleType) {
    case INTERVAL:
        lStatus = mLastCompleteBackup.secsTo(QDateTime::currentDateTimeUtc());
        lInterval = scheduleIntervalInSeconds();
        break;
    case USAGE:
        lStatus = mAccumulatedUsageTime;
        lInterval = mUsageLimit * 3600;
        break;
    }

    if (lStatus < lInterval)
        return GOOD;
    if (lStatus < lInterval * 3)
        return MEDIUM;
    return BAD;
}

QString BackupPlan::iconName(Status pStatus)
{
    switch (pStatus) {
    case GOOD:
        return QStringLiteral("security-high");
    case MEDIUM:
        return QStringLiteral("security-medium");
    case BAD:
        return QStringLiteral("security-low");
    case NO_STATUS:
        break;
    }
    return QString();
}

QString BackupPlan::absoluteExcludesFilePath()
{
    if (QDir::isRelativePath(mExcludePatternsPath)) {
        return QDir::homePath() + QDir::separator() + mExcludePatternsPath;
    }
    return mExcludePatternsPath;
}

void BackupPlan::usrRead()
{
    // correct the time spec after default read routines.
    mLastCompleteBackup.setTimeSpec(Qt::UTC);
    QMutableStringListIterator lExcludes(mPathsExcluded);
    while (lExcludes.hasNext()) {
        ensureNoTrailingSlash(lExcludes.next());
    }
    QMutableStringListIterator lIncludes(mPathsIncluded);
    while (lIncludes.hasNext()) {
        ensureNoTrailingSlash(lIncludes.next());
    }
}

QString BackupPlan::statusText()
{
    QLocale lLocale;
    KFormat lFormat(lLocale);
    QString lStatus;
    if (mLastCompleteBackup.isValid()) {
        QDateTime lLocalTime = mLastCompleteBackup.toLocalTime();

        lStatus += i18nc("%1 is fancy formatted date", "Last saved: %1", lFormat.formatRelativeDate(lLocalTime.date(), QLocale::LongFormat));

        if (mLastBackupSize > 0.0) {
            lStatus += '\n';
            lStatus += i18nc("%1 is storage size of archive", "Size: %1", lFormat.formatByteSize(mLastBackupSize));
        }
        if (mLastAvailableSpace > 0.0) {
            lStatus += '\n';
            lStatus += i18nc("%1 is free storage space", "Free space: %1", lFormat.formatByteSize(mLastAvailableSpace));
        }
    } else {
        lStatus = xi18nc("@label", "This backup plan has never been run.");
    }
    return lStatus;
}
