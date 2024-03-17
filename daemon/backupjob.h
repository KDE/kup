// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

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
    enum ErrorCodes { ErrorWithLog = UserDefinedError, ErrorWithoutLog, ErrorSuggestRepair, ErrorSourcesConfig };

    void start() override;

protected slots:
    virtual void performJob() = 0;

protected:
    BackupJob(BackupPlan &pBackupPlan, QString pDestinationPath, QString pLogFilePath, KupDaemon *pKupDaemon);
    static void makeNice(int pPid);
    static QString quoteArgs(const QStringList &pCommand);
    void jobFinishedSuccess();
    void jobFinishedError(ErrorCodes pErrorCode, const QString &pErrorText);
    BackupPlan &mBackupPlan;
    QString mDestinationPath;
    QString mLogFilePath;
    QFile mLogFile;
    QTextStream mLogStream;
    KupDaemon *mKupDaemon;
};

#endif // BACKUPJOB_H
