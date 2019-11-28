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


#ifndef BACKUPJOB_H
#define BACKUPJOB_H

#include "backupplan.h"

#include <KJob>

#include <QFile>
#include <QStringList>
#include <QTextStream>

class KupDaemon;

class BackupJob : public KJob
{
	Q_OBJECT
public:
	enum ErrorCodes {
		ErrorWithLog = UserDefinedError,
		ErrorWithoutLog,
		ErrorSuggestRepair
	};

	void start() override;

protected slots:
	virtual void performJob() = 0;

protected:
	BackupJob(BackupPlan &pBackupPlan, const QString &pDestinationPath, const QString &pLogFilePath, KupDaemon *pKupDaemon);
	static void makeNice(int pPid);
	QString quoteArgs(const QStringList &pCommand);
	void jobFinishedSuccess();
	void jobFinishedError(ErrorCodes pErrorCode, QString pErrorText);
	BackupPlan &mBackupPlan;
	QString mDestinationPath;
	QString mLogFilePath;
	QFile mLogFile;
	QTextStream mLogStream;
	KupDaemon *mKupDaemon;
};

#endif // BACKUPJOB_H
