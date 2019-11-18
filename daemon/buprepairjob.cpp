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

#include "buprepairjob.h"

#include <KLocalizedString>

BupRepairJob::BupRepairJob(const BackupPlan &pBackupPlan, const QString &pDestinationPath,
                                       const QString &pLogFilePath, KupDaemon *pKupDaemon)
   : BackupJob(pBackupPlan, pDestinationPath, pLogFilePath, pKupDaemon){
	mFsckProcess.setOutputChannelMode(KProcess::SeparateChannels);
}

void BupRepairJob::performJob() {
	KProcess lPar2Process;
	lPar2Process.setOutputChannelMode(KProcess::SeparateChannels);
	lPar2Process << QStringLiteral("bup") << QStringLiteral("fsck") << QStringLiteral("--par2-ok");
	int lExitCode = lPar2Process.execute();
	if(lExitCode < 0) {
		jobFinishedError(ErrorWithoutLog, xi18nc("@info notification",
		                                         "The <application>bup</application> program is needed but could not be found, "
		                                         "maybe it is not installed?"));
		return;
	} else if(mBackupPlan.mGenerateRecoveryInfo && lExitCode != 0) {
		jobFinishedError(ErrorWithoutLog, xi18nc("@info notification",
		                                         "The <application>par2</application> program is needed but could not be found, "
		                                         "maybe it is not installed?"));
		return;
	}

	mLogStream << QStringLiteral("Kup is starting bup repair job at ")
	           << QLocale().toString(QDateTime::currentDateTime())
	           << endl << endl;

	mFsckProcess << QStringLiteral("bup");
	mFsckProcess << QStringLiteral("-d") << mDestinationPath;
	mFsckProcess << QStringLiteral("fsck") << QStringLiteral("-r");

	connect(&mFsckProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(slotRepairDone(int,QProcess::ExitStatus)));
	connect(&mFsckProcess, SIGNAL(started()), SLOT(slotRepairStarted()));
	mLogStream << mFsckProcess.program().join(QStringLiteral(" ")) << endl;
	mFsckProcess.start();
}

void BupRepairJob::slotRepairStarted() {
	makeNice(mFsckProcess.pid());
}

void BupRepairJob::slotRepairDone(int pExitCode, QProcess::ExitStatus pExitStatus) {
	mLogStream << QString::fromUtf8(mFsckProcess.readAllStandardError()) << endl;
	mLogStream << "Exit code: " << pExitCode << endl;
	if(pExitStatus != QProcess::NormalExit) {
		mLogStream << QStringLiteral("Repair failed (the repair process crashed). Your backups could be "
		                             "corrupted! See above for details.") << endl;
		jobFinishedError(ErrorWithLog, xi18nc("@info notification", "Backup repair failed. Your backups could be corrupted! "
		                                                            "See log file for more details."));
	} else if(pExitCode == 100) {
		mLogStream << QStringLiteral("Repair succeeded. See above for details.") << endl;
		jobFinishedError(ErrorWithLog, xi18nc("@info notification", "Success! Backup repair worked. See log file for more details."));
	} else if(pExitCode == 0) {
		mLogStream << QStringLiteral("Repair was not necessary. Your backups are fine. See "
		                             "above for details.") << endl;
		jobFinishedError(ErrorWithLog, xi18nc("@info notification", "Backup repair was not necessary. Your backups are not corrupted. "
		                                                            "See log file for more details."));
	} else {
		mLogStream << QStringLiteral("Repair failed. Your backups could still be "
		                             "corrupted! See above for details.") << endl;
		jobFinishedError(ErrorWithLog, xi18nc("@info notification", "Backup repair failed. Your backups could still be corrupted! "
		                                                            "See log file for more details."));
	}
}
