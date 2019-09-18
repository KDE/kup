/*
 * Copyright 2019 Simon Persson <simon.persson@mykolab.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy 
 * defined in Section 14 of version 3 of the license.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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
