// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

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
