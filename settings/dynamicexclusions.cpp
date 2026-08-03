// SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "dynamicexclusions.h"
#include "kuputils.h"

#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>

#include <KConfig>
#include <Solid/Device>
#include <Solid/StorageAccess>

void DynamicExclusions::setFromPlan(const BackupPlan &pPlan)
{
    mExcludeTrash = pPlan.mExcludeTrash;
    mExcludeAppStates = pPlan.mExcludeAppStates;
    mExcludeCaches = pPlan.mExcludeCaches;
    mExcludeEncryptedMounts = pPlan.mExcludeEncryptedMounts;
    mExcludeSnapshots = pPlan.mExcludeSnapshots;
    mExcludeContainers = pPlan.mExcludeContainers;
    mExcludeUserFlatpaks = pPlan.mExcludeUserFlatpaks;
}

QStringList DynamicExclusions::pathsExcluded(const QStringList &pPathsIncluded)
{
    QStringList lExclusions;

    if (mExcludeEncryptedMounts) {
        const auto lDevices = Solid::Device::listFromQuery(QStringLiteral("StorageAccess.encrypted == true"));
        for (const auto &lDev : lDevices) {
            auto lStorageAccess = lDev.as<Solid::StorageAccess>();
            if (!lStorageAccess || !lStorageAccess->isAccessible()) {
                continue;
            }
            if (pPathsIncluded.contains(lStorageAccess->filePath())) {
                continue;
            }
            lExclusions << lStorageAccess->filePath();
        }

        const KConfig lPlasmaVaultConfig(QStringLiteral("plasmavaultrc"));
        const auto lDeviceKeys = lPlasmaVaultConfig.group(QStringLiteral("EncryptedDevices")).keyList();
        for (const QString &lDeviceKey : lDeviceKeys) {
            const auto lDevice = lPlasmaVaultConfig.group(lDeviceKey);
            if (lDevice.exists()) {
                lExclusions << lDevice.readEntry(QStringLiteral("mountPoint"));
            }
        }
    }

    if (mExcludeCaches) {
        lExclusions << QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
        lExclusions << QDir::homePath() + QStringLiteral("/.thumbnails");
        lExclusions << QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/baloo");
        lExclusions << QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/TelegramDesktop/tdata/temp");
        lExclusions << QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/Riot/cache");

        // Flatpak
        QDirIterator lVarAppIter(QDir::homePath() + QStringLiteral("/.var/app"), QDir::Dirs | QDir::NoDotAndDotDot);
        while (lVarAppIter.hasNext()) {
            lExclusions << lVarAppIter.next() + "/cache";
        }
    }

    if (mExcludeContainers) {
        lExclusions << QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/containers");
        lExclusions << QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/gnome-boxes");
        lExclusions << QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/libvirt");
        lExclusions << QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/docker");
        lExclusions << QDir::homePath() + QStringLiteral("/.var/app/org.gnome.Boxes");
    }

    if (mExcludeAppStates) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        lExclusions << QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation);
#else
        lExclusions << QDir::homePath() + QStringLiteral("/.local/state");
#endif
    }

    if (mExcludeTrash) {
        lExclusions << QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Trash");
    }

    if (mExcludeSnapshots) {
        // this catches snapshots of the home dir taken by Snapper, which stores them under the respective subvolume
        lExclusions << QDir::homePath() + QStringLiteral("/.snapshots");
    }

    if (mExcludeUserFlatpaks) {
        lExclusions << QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/flatpak");
    }

    QMutableStringListIterator lExclusionsIter(lExclusions);
    while (lExclusionsIter.hasNext()) {
        QString &lNext = lExclusionsIter.next();
        lNext = QDir::cleanPath(lNext);
        ensureNoTrailingSlash(lNext);
        if (!QFileInfo::exists(lNext)) {
            lExclusionsIter.remove();
        }
    }

    return lExclusions;
}
