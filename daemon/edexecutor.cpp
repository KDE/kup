// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "edexecutor.h"
#include "backupplan.h"

#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QTimer>
#include <Solid/DeviceInterface>
#include <Solid/DeviceNotifier>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>

EDExecutor::EDExecutor(BackupPlan *pPlan, KupDaemon *pKupDaemon)
    : PlanExecutor(pPlan, pKupDaemon)
    , mStorageAccess(nullptr)
    , mWantsToRunBackup(false)
    , mWantsToShowFiles(false)
    , mWantsToPurge(false)
{
    connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceAdded(QString)), SLOT(deviceAdded(QString)));
    connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceRemoved(QString)), SLOT(deviceRemoved(QString)));
}

void EDExecutor::checkStatus()
{
    QList<Solid::Device> lDeviceList = Solid::Device::listFromType(Solid::DeviceInterface::StorageVolume);
    foreach (const Solid::Device &lDevice, lDeviceList) {
        deviceAdded(lDevice.udi());
    }
    updateAccessibility();
}

void EDExecutor::deviceAdded(const QString &pUdi)
{
    Solid::Device lDevice(pUdi);
    if (!lDevice.is<Solid::StorageVolume>()) {
        return;
    }
    auto *lVolume = lDevice.as<Solid::StorageVolume>();
    QString lUUID = lVolume->uuid();
    if (lUUID.isEmpty()) { // seems to happen for vfat partitions
        Solid::Device lDriveDevice;
        if (lDevice.is<Solid::StorageDrive>()) {
            lDriveDevice = lDevice;
        } else {
            lDriveDevice = lDevice.parent();
        }
        lUUID += lDriveDevice.description();
        lUUID += QStringLiteral("|");
        lUUID += lVolume->label();
    }
    if (mPlan->mExternalUUID == lUUID) {
        mCurrentUdi = pUdi;
        mStorageAccess = lDevice.as<Solid::StorageAccess>();
        enterAvailableState();
    }
}

void EDExecutor::deviceRemoved(const QString &pUdi)
{
    if (mCurrentUdi == pUdi) {
        mWantsToRunBackup = false;
        mCurrentUdi.clear();
        mStorageAccess = nullptr;
        enterNotAvailableState();
    }
}

void EDExecutor::updateAccessibility()
{
    if (mWantsToRunBackup) {
        startBackup(); // run startBackup again now that it has been mounted
    } else if (mWantsToShowFiles) {
        showBackupFiles();
    } else if (mWantsToPurge) {
        showBackupPurger();
    }
}

void EDExecutor::startBackup()
{
    if (!ensureAccessible(mWantsToRunBackup)) {
        exitBackupRunningState(false);
        return;
    }
    PlanExecutor::startBackup();
}

void EDExecutor::showBackupFiles()
{
    if (!ensureAccessible(mWantsToShowFiles)) {
        return;
    }
    PlanExecutor::showBackupFiles();
}

void EDExecutor::showBackupPurger()
{
    if (!ensureAccessible(mWantsToPurge)) {
        return;
    }
    PlanExecutor::showBackupPurger();
}

bool EDExecutor::ensureAccessible(bool &pReturnLater)
{
    pReturnLater = false; // reset in case we are here for the second time
    if (!mStorageAccess) {
        return false;
    }
    if (mStorageAccess->isAccessible()) {
        if (!mStorageAccess->filePath().isEmpty()) {
            mDestinationPath = mStorageAccess->filePath();
            mDestinationPath += QStringLiteral("/");
            mDestinationPath += mPlan->mExternalDestinationPath;
            QDir lDir(mDestinationPath);
            if (!lDir.exists()) {
                lDir.mkpath(mDestinationPath);
            }
            QFileInfo lDestinationInfo(mDestinationPath);
            if (lDestinationInfo.exists() && lDestinationInfo.isDir()) {
                return true;
            }
        }
        return false;
    }
    connect(mStorageAccess, &Solid::StorageAccess::accessibilityChanged, this, &EDExecutor::updateAccessibility);
    mStorageAccess->setup(); // try to mount it, fail silently for now.
    pReturnLater = true;
    return false;
}
