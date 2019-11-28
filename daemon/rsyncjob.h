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

#ifndef RSYNCJOB_H
#define RSYNCJOB_H

#include "backupjob.h"

#include <KProcess>
#include <QElapsedTimer>

class KupDaemon;

class RsyncJob : public BackupJob
{
	Q_OBJECT

public:
	RsyncJob(BackupPlan &pBackupPlan, const QString &pDestinationPath, const QString &pLogFilePath, KupDaemon *pKupDaemon);

protected slots:
	void performJob() override;

protected slots:
	void slotRsyncStarted();
	void slotRsyncFinished(int pExitCode, QProcess::ExitStatus pExitStatus);
	void slotReadRsyncOutput();

protected:
	bool doKill() override;
	bool doSuspend() override;
	bool doResume() override;

	bool performMigration();

	KProcess mRsyncProcess;
	QElapsedTimer mInfoRateLimiter;
};

#endif // RSYNCJOB_H
