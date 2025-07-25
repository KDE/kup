// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "driveselection.h"
#include "backupplan.h"
#include "driveselectiondelegate.h"

#include <QItemSelectionModel>
#include <QList>
#include <QPainter>
#include <QStandardItemModel>
#include <QStorageInfo>
#include <QTimer>

#include <KConfigDialogManager>
#include <KLocalizedString>
#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/StorageAccess>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>

bool deviceLessThan(const Solid::Device &a, const Solid::Device &b)
{
    return a.udi() < b.udi();
}

DriveSelection::DriveSelection(BackupPlan *pBackupPlan, QWidget *parent)
    : QListView(parent)
    , mBackupPlan(pBackupPlan)
    , mSelectedAndAccessible(false)
    , mSyncedBackupType(false)
{
    mDrivesModel = new QStandardItemModel(this);
    setModel(mDrivesModel);
    setItemDelegate(new DriveSelectionDelegate(this));
    setSelectionMode(QAbstractItemView::SingleSelection);
    setWordWrap(true);

    if (!mBackupPlan->mExternalUUID.isEmpty()) {
        auto *lItem = new QStandardItem();
        lItem->setEditable(false);
        lItem->setData(QString(), DriveSelection::UDI);
        lItem->setData(mBackupPlan->mExternalUUID, DriveSelection::UUID);
        lItem->setData(0, DriveSelection::UsedSpace);
        lItem->setData(mBackupPlan->mExternalPartitionNumber, DriveSelection::PartitionNumber);
        lItem->setData(mBackupPlan->mExternalPartitionsOnDrive, DriveSelection::PartitionsOnDrive);
        lItem->setData(mBackupPlan->mExternalDeviceDescription, DriveSelection::DeviceDescription);
        lItem->setData(mBackupPlan->mExternalVolumeCapacity, DriveSelection::TotalSpace);
        lItem->setData(mBackupPlan->mExternalVolumeLabel, DriveSelection::Label);
        mDrivesModel->appendRow(lItem);
    }

    QList<Solid::Device> lDeviceList = Solid::Device::listFromType(Solid::DeviceInterface::StorageDrive);
    foreach (const Solid::Device &lDevice, lDeviceList) {
        deviceAdded(lDevice.udi());
    }
    connect(Solid::DeviceNotifier::instance(), &Solid::DeviceNotifier::deviceAdded, this, &DriveSelection::deviceAdded);
    connect(Solid::DeviceNotifier::instance(), &Solid::DeviceNotifier::deviceRemoved, this, &DriveSelection::deviceRemoved);
    connect(selectionModel(), &QItemSelectionModel::selectionChanged, this, &DriveSelection::updateSelection);
}

QString DriveSelection::mountPathOfSelectedDrive() const
{
    if (mSelectedAndAccessible) {
        QStandardItem *lItem;
        findItem(DriveSelection::UUID, mSelectedUuid, &lItem);
        if (lItem != nullptr) {
            Solid::Device lDevice(lItem->data(DriveSelection::UDI).toString());
            auto *lAccess = lDevice.as<Solid::StorageAccess>();
            if (lAccess) {
                return lAccess->filePath();
            }
        }
    }
    return QString();
}

void DriveSelection::deviceAdded(const QString &pUdi)
{
    Solid::Device lDevice(pUdi);
    if (!lDevice.is<Solid::StorageDrive>()) {
        return;
    }
    auto *lDrive = lDevice.as<Solid::StorageDrive>();
    if (!lDrive->isHotpluggable() && !lDrive->isRemovable()) {
        return;
    }
    if (mDrivesToAdd.contains(pUdi)) {
        return;
    }
    mDrivesToAdd.append(pUdi);
    QTimer::singleShot(2000, this, SLOT(delayedDeviceAdded()));
}

void DriveSelection::delayedDeviceAdded()
{
    if (mDrivesToAdd.isEmpty()) {
        return;
    }
    Solid::Device lParentDevice(mDrivesToAdd.takeFirst());
    QList<Solid::Device> lDeviceList = Solid::Device::listFromType(Solid::DeviceInterface::StorageVolume, lParentDevice.udi());
    // check for when there is no partitioning scheme, then the drive is also the storage volume
    if (lParentDevice.is<Solid::StorageVolume>()) {
        lDeviceList.append(lParentDevice);
    }

    // filter out some volumes that should not be visible
    QList<Solid::Device> lVolumeDeviceList;
    foreach (Solid::Device lVolumeDevice, lDeviceList) {
        auto *lVolume = lVolumeDevice.as<Solid::StorageVolume>();
        if (lVolume && !lVolume->isIgnored() && (lVolume->usage() == Solid::StorageVolume::FileSystem || lVolume->usage() == Solid::StorageVolume::Encrypted)) {
            lVolumeDeviceList.append(lVolumeDevice);
        }
    }

    // simplest attempt at getting the same partition numbering every time a device is plugged in
    std::sort(lVolumeDeviceList.begin(), lVolumeDeviceList.end(), deviceLessThan);

    int lPartitionNumber = 1;
    foreach (Solid::Device lVolumeDevice, lVolumeDeviceList) {
        auto *lVolume = lVolumeDevice.as<Solid::StorageVolume>();
        QString lUuid = lVolume->uuid();
        if (lUuid.isEmpty()) { // seems to happen for vfat partitions
            lUuid += lParentDevice.description();
            lUuid += QStringLiteral("|");
            lUuid += lVolume->label();
        }
        QStandardItem *lItem;
        bool lNeedsToBeAdded = false;
        findItem(DriveSelection::UUID, lUuid, &lItem);
        if (lItem == nullptr) {
            lItem = new QStandardItem();
            lItem->setEditable(false);
            lItem->setData(lUuid, DriveSelection::UUID);
            lItem->setData(0, DriveSelection::TotalSpace);
            lItem->setData(0, DriveSelection::UsedSpace);
            lNeedsToBeAdded = true;
        }
        lItem->setData(lParentDevice.description(), DriveSelection::DeviceDescription);
        lItem->setData(lVolume->label(), DriveSelection::Label);
        lItem->setData(lVolumeDevice.udi(), DriveSelection::UDI);
        lItem->setData(lPartitionNumber, DriveSelection::PartitionNumber);
        lItem->setData(lVolumeDeviceList.count(), DriveSelection::PartitionsOnDrive);
        lItem->setData(lVolume->fsType(), DriveSelection::FileSystem);
        lItem->setData(mSyncedBackupType && (lVolume->fsType() == QStringLiteral("vfat") || lVolume->fsType() == QStringLiteral("exfat") || lVolume->fsType() == QStringLiteral("ntfs")),
                       DriveSelection::PermissionLossWarning);
        lItem->setData(mSyncedBackupType && (lVolume->fsType() == QStringLiteral("vfat") || lVolume->fsType() == QStringLiteral("exfat")),
                       DriveSelection::SymlinkLossWarning);

        auto *lAccess = lVolumeDevice.as<Solid::StorageAccess>();
        connect(lAccess, &Solid::StorageAccess::accessibilityChanged, this, &DriveSelection::accessabilityChanged);
        if (lAccess->isAccessible()) {
            QStorageInfo storageInfo(lAccess->filePath());
            if (storageInfo.isValid()) {
                lItem->setData(storageInfo.bytesTotal(), DriveSelection::TotalSpace);
                lItem->setData(storageInfo.bytesTotal() - storageInfo.bytesFree(), DriveSelection::UsedSpace);
            }
            if (lUuid == mSelectedUuid) {
                // Selected volume was just added, could not have been accessible before.
                mSelectedAndAccessible = true;
                emit selectedDriveIsAccessibleChanged(true);
            }
        } else if (lVolume->usage() != Solid::StorageVolume::Encrypted) {
            // Don't bother the user with password prompt just for sake of storage volume space info
            lAccess->setup();
        }
        if (lNeedsToBeAdded) {
            mDrivesModel->appendRow(lItem);
            if (mDrivesModel->rowCount() == 1) {
                selectionModel()->select(mDrivesModel->index(0, 0), QItemSelectionModel::ClearAndSelect);
            }
        }
        lPartitionNumber++;
    }
}

void DriveSelection::deviceRemoved(const QString &pUdi)
{
    QStandardItem *lItem;
    int lRow = findItem(DriveSelection::UDI, pUdi, &lItem);
    if (lRow >= 0) {
        QString lUuid = lItem->data(DriveSelection::UUID).toString();
        if (lUuid == mBackupPlan->mExternalUUID) {
            // let the selected and saved item stay in the list
            // just clear the UDI so that it will be shown as disconnected.
            lItem->setData(QString(), DriveSelection::UDI);
        } else {
            mDrivesModel->removeRow(lRow);
        }
        if (lUuid == mSelectedUuid && mSelectedAndAccessible) {
            mSelectedAndAccessible = false;
            emit selectedDriveIsAccessibleChanged(false);
        }
    }
}

void DriveSelection::accessabilityChanged(bool pAccessible, const QString &pUdi)
{
    QStandardItem *lItem;
    findItem(DriveSelection::UDI, pUdi, &lItem);
    if (lItem != nullptr) {
        if (pAccessible) {
            Solid::Device lDevice(pUdi);
            auto *lAccess = lDevice.as<Solid::StorageAccess>();
            if (lAccess) {
                QStorageInfo storageInfo(lAccess->filePath());
                if (storageInfo.isValid()) {
                    lItem->setData(storageInfo.bytesTotal(), DriveSelection::TotalSpace);
                    lItem->setData(storageInfo.bytesTotal() - storageInfo.bytesFree(), DriveSelection::UsedSpace);
                }
            }
        }
        bool lSelectedAndAccessible = (lItem->data(DriveSelection::UUID).toString() == mSelectedUuid && pAccessible);
        if (lSelectedAndAccessible != mSelectedAndAccessible) {
            mSelectedAndAccessible = lSelectedAndAccessible;
            emit selectedDriveIsAccessibleChanged(lSelectedAndAccessible);
        }
    }
}

void DriveSelection::updateSelection(const QItemSelection &pSelected, const QItemSelection &pDeselected)
{
    Q_UNUSED(pDeselected)
    if (!pSelected.indexes().isEmpty()) {
        QModelIndex lIndex = pSelected.indexes().first();
        if (mSelectedUuid.isEmpty()) {
            emit driveIsSelectedChanged(true);
        }
        mSelectedUuid = lIndex.data(DriveSelection::UUID).toString();
        emit selectedDriveChanged(mSelectedUuid);
        // check if the newly selected volume is accessible, compare to previous selection
        bool lIsAccessible = false;
        QString lUdiOfSelected = lIndex.data(DriveSelection::UDI).toString();
        if (!lUdiOfSelected.isEmpty()) {
            Solid::Device lDevice(lUdiOfSelected);
            auto *lAccess = lDevice.as<Solid::StorageAccess>();
            if (lAccess != nullptr) {
                lIsAccessible = lAccess->isAccessible();
            }
        }
        if (mSelectedAndAccessible != lIsAccessible) {
            mSelectedAndAccessible = lIsAccessible;
            emit selectedDriveIsAccessibleChanged(mSelectedAndAccessible);
        }
    } else {
        mSelectedUuid.clear();
        emit selectedDriveChanged(mSelectedUuid);
        emit driveIsSelectedChanged(false);
        mSelectedAndAccessible = false;
        emit selectedDriveIsAccessibleChanged(false);
    }
}

void DriveSelection::paintEvent(QPaintEvent *pPaintEvent)
{
    QListView::paintEvent(pPaintEvent);
    if (mDrivesModel->rowCount() == 0) {
        QPainter lPainter(viewport());
        style()->drawItemText(&lPainter,
                              rect(),
                              Qt::AlignCenter,
                              palette(),
                              false,
                              xi18nc("@label Only shown if no drives are detected",
                                     "Plug in the external "
                                     "storage you wish to use, then select it in this list."),
                              QPalette::Text);
    }
}

void DriveSelection::setSelectedDrive(const QString &pUuid)
{
    if (pUuid == mSelectedUuid) {
        return;
    }
    if (pUuid.isEmpty()) {
        clearSelection();
    } else {
        QStandardItem *lItem;
        findItem(DriveSelection::UUID, pUuid, &lItem);
        if (lItem != nullptr) {
            setCurrentIndex(mDrivesModel->indexFromItem(lItem));
        }
    }
}

void DriveSelection::saveExtraData()
{
    QStandardItem *lItem;
    findItem(DriveSelection::UUID, mSelectedUuid, &lItem);
    if (lItem != nullptr) {
        mBackupPlan->mExternalDeviceDescription = lItem->data(DriveSelection::DeviceDescription).toString();
        mBackupPlan->mExternalPartitionNumber = lItem->data(DriveSelection::PartitionNumber).toInt();
        mBackupPlan->mExternalPartitionsOnDrive = lItem->data(DriveSelection::PartitionsOnDrive).toInt();
        mBackupPlan->mExternalVolumeCapacity = lItem->data(DriveSelection::TotalSpace).toULongLong();
        mBackupPlan->mExternalVolumeLabel = lItem->data(DriveSelection::Label).toString();
    }
}

void DriveSelection::updateSyncWarning(bool pSyncBackupSelected)
{
    mSyncedBackupType = pSyncBackupSelected;
    for (int i = 0; i < mDrivesModel->rowCount(); ++i) {
        QString lFsType = mDrivesModel->item(i)->data(DriveSelection::FileSystem).toString();
        mDrivesModel->item(i)->setData(mSyncedBackupType && (lFsType == QStringLiteral("vfat") || lFsType == QStringLiteral("exfat") || lFsType == QStringLiteral("ntfs")),
                                       DriveSelection::PermissionLossWarning);
        mDrivesModel->item(i)->setData(mSyncedBackupType && (lFsType == QStringLiteral("vfat") || lFsType == QStringLiteral("exfat")),
                                       DriveSelection::SymlinkLossWarning);
    }
}

int DriveSelection::findItem(const DriveSelection::DataType pField, const QString &pSearchString, QStandardItem **pReturnedItem) const
{
    for (int lRow = 0; lRow < mDrivesModel->rowCount(); ++lRow) {
        QStandardItem *lItem = mDrivesModel->item(lRow);
        if (lItem->data(pField).toString() == pSearchString) {
            if (pReturnedItem != nullptr) {
                *pReturnedItem = lItem;
            }
            return lRow;
        }
    }
    if (pReturnedItem != nullptr) {
        *pReturnedItem = nullptr;
    }
    return -1;
}
