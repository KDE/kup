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


#ifndef BUPJOB_H
#define BUPJOB_H

#include "backupjob.h"

#include <KProcess>
#include <QElapsedTimer>

class KupDaemon;

class BupJob : public BackupJob
{
	Q_OBJECT

public:
	BupJob(BackupPlan &pBackupPlan, const QString &pDestinationPath, const QString &pLogFilePath, KupDaemon *pKupDaemon);

protected slots:
	void performJob() override;
	void slotCheckingStarted();
	void slotCheckingDone(int pExitCode, QProcess::ExitStatus pExitStatus);
	void slotIndexingStarted();
	void slotIndexingDone(int pExitCode, QProcess::ExitStatus pExitStatus);
	void slotSavingStarted();
	void slotSavingDone(int pExitCode, QProcess::ExitStatus pExitStatus);
	void slotRecoveryInfoStarted();
	void slotRecoveryInfoDone(int pExitCode, QProcess::ExitStatus pExitStatus);
	void slotReadBupErrors();

protected:
	bool doSuspend() override;
	bool doResume() override;

	KProcess mFsckProcess;
	KProcess mIndexProcess;
	KProcess mSaveProcess;
	KProcess mPar2Process;
	QElapsedTimer mInfoRateLimiter;
	int mHarmlessErrorCount;
	bool mAllErrorsHarmless;
};

#endif /*BUPJOB_H*/
