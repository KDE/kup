// SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "externaldrivebackupsmodel.h"

#include <QDirIterator>
#include <QProcess>
#include <QString>
#include <QtConcurrentRun>

#include <Solid/Device>
#include <Solid/DeviceInterface>
#include <Solid/DeviceNotifier>
#include <Solid/StorageAccess>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>

using namespace Qt::StringLiterals;

bool isBackup(const QString &pDirPath)
{
    return QFileInfo::exists(QDir(pDirPath).filePath("bupindex"_L1));
}

void scanDirectory(QPromise<QString> &pPromise, const QString &pRootPath)
{
    if (isBackup(pRootPath)) {
        pPromise.addResult(pRootPath);
    }

    QDirIterator lIt(pRootPath, QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);

    while (lIt.hasNext()) {
        if (pPromise.isCanceled()) {
            return;
        }

        QString lCurrentDir = lIt.next();
        if (isBackup(lCurrentDir)) {
            pPromise.addResult(lCurrentDir);
        }
    }
}

ExternalDriveBackupsModel::ExternalDriveBackupsModel(QObject *pParent)
    : QAbstractItemModel(pParent)
{
    const auto lDeviceList = Solid::Device::listFromType(Solid::DeviceInterface::StorageDrive);
    for (const auto &lDevice : lDeviceList) {
        addDrive(lDevice.udi());
    }

    connect(Solid::DeviceNotifier::instance(), &Solid::DeviceNotifier::deviceAdded, this, &ExternalDriveBackupsModel::addDrive);
    connect(Solid::DeviceNotifier::instance(), &Solid::DeviceNotifier::deviceRemoved, this, &ExternalDriveBackupsModel::removeDrive);
}

ExternalDriveBackupsModel::~ExternalDriveBackupsModel()
{
    // TODO cancel futures here
}

void ExternalDriveBackupsModel::addDrive(const QString &pUdi)
{
    Solid::Device lDevice(pUdi);
    QList<Solid::Device> lVolumes;

    if (lDevice.is<Solid::StorageDrive>()) {
        const auto *lDrive = lDevice.as<Solid::StorageDrive>();
        if (!lDrive->isHotpluggable() && !lDrive->isRemovable()) {
            return;
        }
        lVolumes << Solid::Device::listFromType(Solid::DeviceInterface::StorageVolume, pUdi);
        if (lDevice.is<Solid::StorageVolume>()) {
            lVolumes << lDevice;
        }
    } else if (lDevice.is<Solid::StorageVolume>()) {
        const Solid::Device lParentDevice = lDevice.parent();
        if (!lParentDevice.is<Solid::StorageDrive>()) {
            return;
        }
        const auto *lDrive = lParentDevice.as<Solid::StorageDrive>();
        if (!lDrive->isHotpluggable() && !lDrive->isRemovable()) {
            return;
        }
        lVolumes << lDevice;
    }

    QStringList lValidVolumes;
    for (const auto &lVolumeDevice : lVolumes) {
        if (mDrives.contains(lVolumeDevice.udi())) {
            continue;
        }
        const auto *lVolume = lVolumeDevice.as<Solid::StorageVolume>();
        if (lVolume->isIgnored()) {
            continue;
        }
        if (lVolume->usage() == Solid::StorageVolume::FileSystem || lVolume->usage() == Solid::StorageVolume::Encrypted) {
            lValidVolumes << lVolumeDevice.udi();
        }
    }

    beginInsertRows({}, mDrives.size(), mDrives.size() + lValidVolumes.size() - 1);
    mDrives << lValidVolumes;
    endInsertRows();
}

void ExternalDriveBackupsModel::removeDrive(const QString &pUdi)
{
    const auto lIdx = mDrives.indexOf(pUdi);
    if (lIdx == -1) {
        return;
    }

    beginRemoveRows({}, lIdx, lIdx);
    mDrives.removeAt(lIdx);
    endRemoveRows();

    mDriveBackups.remove(pUdi);
    auto lWatcher = mDriveSearchWatchers.value(pUdi, nullptr);
    if (lWatcher) {
        lWatcher->cancel();
        lWatcher->deleteLater();
    }
    mDriveSearchWatchers.remove(pUdi);
}

int ExternalDriveBackupsModel::rowCount(const QModelIndex &pParent) const
{
    if (!pParent.isValid()) {
        return mDrives.size();
    }

    if (pParent.internalId() == 0) {
        const auto lParentIdx = pParent.row();
        if (lParentIdx < mDrives.size()) {
            const auto &lUdi = mDrives.at(lParentIdx);
            return mDriveBackups.value(lUdi, {}).size();
        }
    }

    return 0;
}

int ExternalDriveBackupsModel::columnCount(const QModelIndex &pParent) const
{
    Q_UNUSED(pParent);
    return 1;
}

QModelIndex ExternalDriveBackupsModel::index(int pRow, int pColumn, const QModelIndex &pParent) const
{
    if (!hasIndex(pRow, pColumn, pParent)) {
        return QModelIndex();
    }

    if (!pParent.isValid()) {
        return createIndex(pRow, pColumn, quintptr(0));
    }

    if (pParent.internalId() == 0) {
        return createIndex(pRow, pColumn, static_cast<quintptr>(pParent.row() + 1));
    }

    return QModelIndex();
}

QModelIndex ExternalDriveBackupsModel::parent(const QModelIndex &pIndex) const
{
    if (!pIndex.isValid()) {
        return QModelIndex();
    }

    if (pIndex.internalId() == 0) {
        return QModelIndex();
    }

    int lParentRow = static_cast<int>(pIndex.internalId()) - 1;
    if (lParentRow < 0 || lParentRow >= mDrives.size()) {
        return QModelIndex();
    }

    return createIndex(lParentRow, 0, quintptr(0));
}

bool ExternalDriveBackupsModel::hasChildren(const QModelIndex &pParent) const
{
    if (pParent.isValid() && pParent.internalId() == 0 && pParent.row() < mDrives.size()) {
        const QString &lUdi = mDrives.at(pParent.row());
        const auto lWatcher = mDriveSearchWatchers.value(lUdi, nullptr);
        if (lWatcher && lWatcher->isFinished() && mDriveBackups.value(lUdi, {}).size() == 0) {
            return false;
        }
        return true;
    }

    return false;
}

bool ExternalDriveBackupsModel::canFetchMore(const QModelIndex &pParent) const
{
    if (pParent.isValid() && pParent.internalId() == 0 && pParent.row() < mDrives.size()) {
        const QString &lUdi = mDrives.at(pParent.row());
        return !mDriveSearchWatchers.contains(lUdi);
    }

    return false;
}

void ExternalDriveBackupsModel::fetchMore(const QModelIndex &pParent)
{
    if (!pParent.isValid() || pParent.internalId() != 0 || pParent.row() >= mDrives.size()) {
        return;
    }

    const QString &lUdi = mDrives.at(pParent.row());
    Solid::Device lDevice(lUdi);
    auto *lAccess = lDevice.as<Solid::StorageAccess>();
    if (lAccess->isAccessible()) {
        driveMounted(lUdi, lAccess->filePath());
    } else {
        connect(lAccess, &Solid::StorageAccess::setupDone, this, [this, lUdi, lAccess](Solid::ErrorType pErr) {
            if (pErr != Solid::ErrorType::NoError) {
                return;
            }
            driveMounted(lUdi, lAccess->filePath());
        });
        lAccess->setup();
    }
}

void ExternalDriveBackupsModel::driveMounted(const QString &pUdi, const QString &pMountPath)
{
    auto lWatcher = new QFutureWatcher<QString>();
    connect(lWatcher, &QFutureWatcher<QString>::resultReadyAt, this, [this, lWatcher, pUdi](int pIndex) {
        QString lRes = lWatcher->resultAt(pIndex);
        listBupRepo(pUdi, lRes);
    });

    const auto lDriveIdx = createIndex(mDrives.indexOf(pUdi), 0);
    Q_EMIT dataChanged(lDriveIdx, lDriveIdx, {Roles::DriveIsScanningRole});
    lWatcher->setFuture(QtConcurrent::run(&scanDirectory, pMountPath));
    connect(lWatcher, &QFutureWatcher<QString>::finished, this, [this, lDriveIdx] {
        Q_EMIT dataChanged(lDriveIdx, lDriveIdx, {Roles::DriveIsScanningRole});
    });
    mDriveSearchWatchers.insert(pUdi, std::move(lWatcher));
}

QVariant ExternalDriveBackupsModel::data(const QModelIndex &pIndex, int pRole) const
{
    if (!pIndex.isValid()) {
        return {};
    }

    if (pIndex.internalId() == 0) {
        if (pIndex.row() >= mDrives.size()) {
            return {};
        }

        const QString &lUdi = mDrives.at(pIndex.row());
        Solid::Device lDevice(lUdi);
        const auto *lAccess = lDevice.as<Solid::StorageAccess>();

        switch (pRole) {
        case Qt::DisplayRole:
        case Roles::DriveDescriptionRole:
            return lDevice.displayName();
        case Roles::DriveUDIRole:
            return lUdi;
        case Roles::DriveIsMountedRole:
            return lAccess->isAccessible();
        case Roles::DriveMountPathRole:
            return lAccess->filePath();
        case Roles::DriveIsScanningRole: {
            const auto lWatcher = mDriveSearchWatchers.value(lUdi, nullptr);
            return lWatcher && lWatcher->isRunning();
        }
        default:
            return {};
        }
    }

    int lParentRow = static_cast<int>(pIndex.internalId()) - 1;
    if (lParentRow < 0 || lParentRow >= mDrives.size()) {
        return {};
    }

    const QString &lDriveUdi = mDrives.at(lParentRow);
    const auto &lBackups = mDriveBackups.value(lDriveUdi, {});

    if (pIndex.row() >= lBackups.size()) {
        return {};
    }

    const auto &lBackup = lBackups.at(pIndex.row());
    switch (pRole) {
    case Qt::DisplayRole:
        return lBackup.mDate.toString();
    case Roles::BackupUsernameRole:
        return lBackup.mUsername;
    case Roles::BackupDateRole:
        return lBackup.mDate;
    case Roles::BackupFSPathRole:
        return lBackup.mFsPath;
    case Roles::BackupRelativeFSPathRole: {
        Solid::Device lDevice(lDriveUdi);
        const auto *lAccess = lDevice.as<Solid::StorageAccess>();
        return QDir(lAccess->filePath()).relativeFilePath(lBackup.mFsPath);
    }
    case Roles::BackupRevisionRole:
        return lBackup.mDatestamp;
    case Roles::BackupNameRole:
        return lBackup.mBupName;
    }

    return {};
}

QHash<int, QByteArray> ExternalDriveBackupsModel::roleNames() const
{
    return {
        {Qt::DisplayRole, "display"_ba},
        {Roles::DriveDescriptionRole, "description"_ba},
        {Roles::DriveUDIRole, "udi"_ba},
        {Roles::DriveIsMountedRole, "isMounted"_ba},
        {Roles::DriveMountPathRole, "mountPath"_ba},
        {Roles::DriveIsScanningRole, "isScanning"_ba},
        {Roles::BackupUsernameRole, "username"_ba},
        {Roles::BackupDateRole, "date"_ba},
        {Roles::BackupFSPathRole, "fsPath"_ba},
        {Roles::BackupRelativeFSPathRole, "relativeFsPath"_ba},
        {Roles::BackupNameRole, "name"_ba},
        {Roles::BackupRevisionRole, "revision"_ba},
    };
}

void ExternalDriveBackupsModel::listBupRepo(const QString &pDriveUdi, const QString &pRepoPath)
{
    QProcess *lBupLsTopLevelProcess = new QProcess(this);
    QObject::connect(lBupLsTopLevelProcess,
                     &QProcess::finished,
                     this,
                     [this, lBupLsTopLevelProcess, pDriveUdi, pRepoPath](int pExitCode, QProcess::ExitStatus pExitStatus) {
                         if (pExitStatus != QProcess::NormalExit || pExitCode != 0) {
                             qDebug() << "bupLsTopLevelProcess fail" << pExitCode;
                             qDebug() << lBupLsTopLevelProcess->readAllStandardError();
                             qDebug() << lBupLsTopLevelProcess->readAllStandardOutput();
                             return;
                         }
                         const QStringList lBupNames = QString::fromUtf8(lBupLsTopLevelProcess->readAllStandardOutput()).split('\n'_L1, Qt::SkipEmptyParts);
                         for (const auto &lName : lBupNames) {
                             listBupRepoBackups(pDriveUdi, pRepoPath, lName);
                         }
                         lBupLsTopLevelProcess->deleteLater();
                     });
    lBupLsTopLevelProcess->start("bup"_L1, {"--bup-dir"_L1, pRepoPath, "ls"_L1});
}

void ExternalDriveBackupsModel::listBupRepoBackups(const QString &pDriveUdi, const QString &pRepoPath, const QString &pName)
{
    QProcess *lBupLsRevisionsProcess = new QProcess(this);
    QObject::connect(lBupLsRevisionsProcess,
                     &QProcess::finished,
                     this,
                     [this, lBupLsRevisionsProcess, pDriveUdi, pRepoPath, pName](int pExitCode, QProcess::ExitStatus pExitStatus) {
                         if (pExitStatus != QProcess::NormalExit || pExitCode != 0) {
                             qDebug() << "bupLsRevisionsProcess fail";
                             return;
                         }
                         const QStringList lRevisions = QString::fromUtf8(lBupLsRevisionsProcess->readAllStandardOutput()).split('\n'_L1, Qt::SkipEmptyParts);
                         for (const auto &lRevision : lRevisions) {
                             if (lRevision == "latest"_L1) {
                                 continue;
                             }
                             listBupRepoDate(pDriveUdi, pRepoPath, pName, lRevision);
                         }
                         lBupLsRevisionsProcess->deleteLater();
                     });
    lBupLsRevisionsProcess->start("bup"_L1, {"--bup-dir"_L1, pRepoPath, "ls"_L1, pName});
}

void ExternalDriveBackupsModel::listBupRepoDate(const QString &pDriveUdi, const QString &pRepoPath, const QString &pName, const QString &pDatestamp)
{
    QProcess *lBupLsUserHomesProcess = new QProcess(this);
    QObject::connect(lBupLsUserHomesProcess,
                     &QProcess::finished,
                     this,
                     [this, lBupLsUserHomesProcess, pDriveUdi, pRepoPath, pName, pDatestamp](int pExitCode, QProcess::ExitStatus pExitStatus) {
                         if (pExitStatus != QProcess::NormalExit || pExitCode != 0) {
                             qDebug() << "bupLsUserHomesProcess fail";
                             return;
                         }
                         const QStringList lUsers = QString::fromUtf8(lBupLsUserHomesProcess->readAllStandardOutput()).split('\n'_L1, Qt::SkipEmptyParts);
                         for (const auto &lUser : lUsers) {
                             addHomeBackup(pDriveUdi, pRepoPath, pName, pDatestamp, lUser);
                         }
                         lBupLsUserHomesProcess->deleteLater();
                     });
    lBupLsUserHomesProcess->start("bup"_L1, {"--bup-dir"_L1, pRepoPath, "ls"_L1, "%1/%2/home"_L1.arg(pName, pDatestamp)});
}

void ExternalDriveBackupsModel::addHomeBackup(const QString &pDriveUdi,
                                              const QString &pRepoPath,
                                              const QString &pName,
                                              const QString &pDatestamp,
                                              const QString &pUsername)
{
    struct HomeBackup lBackupInfo;
    lBackupInfo.mFsPath = pRepoPath;
    lBackupInfo.mBupName = pName;
    lBackupInfo.mDate = QDateTime::fromString(pDatestamp, "yyyy-MM-dd-hhmmss"_L1, QTimeZone::LocalTime);
    lBackupInfo.mDatestamp = pDatestamp;
    lBackupInfo.mUsername = pUsername;

    int lParentRow = mDrives.indexOf(pDriveUdi);
    if (lParentRow != -1) {
        if (!mDriveBackups.contains(pDriveUdi)) {
            mDriveBackups.insert(pDriveUdi, {});
        }
        auto &lBackups = mDriveBackups[pDriveUdi];
        // backups may be listed in arbitrary order
        // so we insert them in sorted order here
        auto lIt =
            std::lower_bound(lBackups.begin(), lBackups.end(), lBackupInfo.mDate.toMSecsSinceEpoch(), [](const struct HomeBackup &pBackup, qint64 pNewVal) {
                return pBackup.mDate.toMSecsSinceEpoch() < pNewVal;
            });
        auto lIdx = std::distance(lBackups.begin(), lIt);
        if (lBackups.size() > lIdx && lBackups.at(lIdx) == lBackupInfo) {
            return;
        }
        beginInsertRows(createIndex(lParentRow, 0, quintptr(0)), lIdx, lIdx);
        lBackups.insert(lIt, lBackupInfo);
        endInsertRows();
    }
}
