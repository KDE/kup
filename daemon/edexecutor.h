// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef EDEXECUTOR_H
#define EDEXECUTOR_H

#include "planexecutor.h"

#include <Solid/Device>
#include <Solid/StorageAccess>

class BackupPlan;

// Plan executor that stores the backup to an external disk.
// Uses libsolid to monitor for when it becomes available.
class EDExecutor : public PlanExecutor
{
    Q_OBJECT

public:
    EDExecutor(BackupPlan *pPlan, KupDaemon *pKupDaemon);

public slots:
    void checkStatus() override;
    void showBackupFiles() override;
    void showBackupPurger() override;

protected slots:
    void deviceAdded(const QString &pUdi);
    void deviceRemoved(const QString &pUdi);
    void updateAccessibility();
    void startBackup() override;

protected:
    bool ensureAccessible(bool &pReturnLater);
    Solid::StorageAccess *mStorageAccess;
    QString mCurrentUdi;
    bool mWantsToRunBackup;
    bool mWantsToShowFiles;
    bool mWantsToPurge;
};

#endif // EDEXECUTOR_H
