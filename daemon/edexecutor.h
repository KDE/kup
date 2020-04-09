// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL

#ifndef EDEXECUTOR_H
#define EDEXECUTOR_H

#include "planexecutor.h"

#include <Solid/Device>
#include <Solid/StorageAccess>

class BackupPlan;

class KJob;

// Plan executor that stores the backup to an external disk.
// Uses libsolid to monitor for when it becomes available.
class EDExecutor: public PlanExecutor
{
Q_OBJECT

public:
	EDExecutor(BackupPlan *pPlan, KupDaemon *pKupDaemon);

public slots:
	void checkStatus() override;
	void showBackupFiles() override;

protected slots:
	void deviceAdded(const QString &pUdi);
	void deviceRemoved(const QString &pUdi);
	void updateAccessibility();
	void startBackup() override;
	void slotBackupDone(KJob *pJob);
	void slotBackupSizeDone(KJob *pJob);

protected:
	Solid::StorageAccess *mStorageAccess;
	QString mCurrentUdi;
	bool mWantsToRunBackup;
	bool mWantsToShowFiles;
};

#endif // EDEXECUTOR_H
