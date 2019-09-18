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

#ifndef FSEXECUTOR_H
#define FSEXECUTOR_H

#include "planexecutor.h"

#include <QThread>

class BackupPlan;

class KDirWatch;
class KJob;

class QTimer;

// KDirWatch (well, inotify) does not detect when something gets mounted on a watched directory.
// work around this problem by monitoring the mounts of the system in a separate thread.
class MountWatcher: public QThread {
	Q_OBJECT

signals:
	void mountsChanged();

protected:
	void run() override;
};


// Plan executor that stores the backup to a path in the local
// filesystem, uses KDirWatch to monitor for when the folder
// becomes available/unavailable. Can be used for external
// drives or networked filesystems if you always mount it at
// the same mountpoint.
class FSExecutor: public PlanExecutor
{
Q_OBJECT

public:
	FSExecutor(BackupPlan *pPlan, KupDaemon *pKupDaemon);
	~FSExecutor() override;

public slots:
	void checkStatus() override;

protected slots:
	void startBackup() override;
	void slotBackupDone(KJob *pJob);
	void slotBackupSizeDone(KJob *pJob);
	void checkMountPoints();

protected:
	QString mWatchedParentDir;
	KDirWatch *mDirWatch;
	MountWatcher mMountWatcher;
};

#endif // FSEXECUTOR_H
