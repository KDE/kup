// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "backupplan.h"
#include "kuputils.h"

#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>
#include <QString>
#include <QTimeZone>

#include <KFormat>
#include <KLocalizedString>

#include <utility>

BackupPlan::BackupPlan(int pPlanNumber, KSharedConfigPtr pConfig, QObject *pParent)
    : BackupPlanSettings(std::move(pConfig), pPlanNumber, pParent)
    , mPlanNumber(pPlanNumber)
{
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
    setDescription(i18nc("default description of newly duplicated backup plan", "%1 (copy)", pPlan.description()));
    setPathsIncluded(pPlan.pathsIncluded());
    setPathsExcluded(pPlan.pathsExcluded());
    setBackupType(pPlan.backupType());
    setScheduleType(pPlan.scheduleType());
    setScheduleInterval(pPlan.scheduleInterval());
    setScheduleIntervalUnit(pPlan.scheduleIntervalUnit());
    setUsageLimit(pPlan.usageLimit());
    setAskBeforeTakingBackup(pPlan.askBeforeTakingBackup());
    setDestinationType(pPlan.destinationType());
    setFilesystemDestinationPath(pPlan.filesystemDestinationPath());
    setExternalUUID(pPlan.externalUUID());
    setExternalDestinationPath(pPlan.externalDestinationPath());
    setExternalVolumeLabel(pPlan.externalVolumeLabel());
    setExternalDeviceDescription(pPlan.externalDeviceDescription());
    setExternalPartitionNumber(pPlan.externalPartitionNumber());
    setExternalPartitionsOnDrive(pPlan.externalPartitionsOnDrive());
    setExternalVolumeCapacity(pPlan.externalVolumeCapacity());
    setShowHiddenFolders(pPlan.showHiddenFolders());
    setGenerateRecoveryInfo(pPlan.generateRecoveryInfo());
    setCheckBackups(pPlan.checkBackups());
    setExcludeTrash(pPlan.excludeTrash());
    setExcludeAppStates(pPlan.excludeAppStates());
    setExcludeCaches(pPlan.excludeCaches());
    setExcludeEncryptedMounts(pPlan.excludeEncryptedMounts());
    setExcludeSnapshots(pPlan.excludeSnapshots());
    setExcludeContainers(pPlan.excludeContainers());
    setExcludeUserFlatpaks(pPlan.excludeUserFlatpaks());
}

QDateTime BackupPlan::nextScheduledTime()
{
    Q_ASSERT(scheduleType() == 1);
    if (!lastCompleteBackup().isValid())
        return QDateTime(); // plan has never been run

    return lastCompleteBackup().addSecs(scheduleIntervalInSeconds());
}

qint64 BackupPlan::scheduleIntervalInSeconds() const
{
    Q_ASSERT(scheduleType() == 1);

    switch (scheduleIntervalUnit()) {
    case 0:
        return scheduleInterval() * 60;
    case 1:
        return scheduleInterval() * 60 * 60;
    case 2:
        return scheduleInterval() * 60 * 60 * 24;
    case 3:
        return scheduleInterval() * 60 * 60 * 24 * 7;
    default:
        return 0;
    }
}

Status BackupPlan::backupStatus() const
{
    if (!lastCompleteBackup().isValid()) {
        return BAD;
    }
    if (scheduleType() == MANUAL) {
        return NO_STATUS;
    }

    qint64 lStatus = 5; // trigger BAD status if schedule type is something strange
    qint64 lInterval = 1;

    switch (scheduleType()) {
    case INTERVAL:
        lStatus = lastCompleteBackup().secsTo(QDateTime::currentDateTimeUtc());
        lInterval = scheduleIntervalInSeconds();
        break;
    case USAGE:
        lStatus = accumulatedUsageTime();
        lInterval = static_cast<qint64>(usageLimit()) * 3600;
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

QString BackupPlan::statusIconName() const
{
    return iconName(backupStatus());
}

QString BackupPlan::absoluteExcludesFilePath()
{
    if (QDir::isRelativePath(excludePatternsPath())) {
        return QDir::homePath() + QDir::separator() + excludePatternsPath();
    }
    return excludePatternsPath();
}

void BackupPlan::usrRead()
{
    // correct the time spec after default read routines.
    QDateTime lLastBackup = lastCompleteBackup();
    lLastBackup.setTimeZone(QTimeZone::utc());
    setLastCompleteBackup(lLastBackup);

    QStringList lExcludes = pathsExcluded();
    for (QString &lPath : lExcludes) {
        ensureNoTrailingSlash(lPath);
    }
    setPathsExcluded(lExcludes);

    QStringList lIncludes = pathsIncluded();
    for (QString &lPath : lIncludes) {
        ensureNoTrailingSlash(lPath);
    }
    setPathsIncluded(lIncludes);
}

QString BackupPlan::statusText() const
{
    QLocale lLocale;
    KFormat lFormat(lLocale);
    QString lStatus;
    if (lastCompleteBackup().isValid()) {
        QDateTime lLocalTime = lastCompleteBackup().toLocalTime();

        lStatus += i18nc("%1 is fancy formatted date", "Last saved: %1", lFormat.formatRelativeDate(lLocalTime.date(), QLocale::LongFormat));

        if (lastBackupSize() > 0.0) {
            lStatus += '\n';
            lStatus += i18nc("%1 is storage size of archive", "Size: %1", lFormat.formatByteSize(lastBackupSize()));
        }
        if (lastAvailableSpace() > 0.0) {
            lStatus += '\n';
            lStatus += i18nc("%1 is free storage space", "Free space: %1", lFormat.formatByteSize(lastAvailableSpace()));
        }
    } else {
        lStatus = xi18nc("@label", "This backup plan has never been run.");
    }
    return lStatus;
}
