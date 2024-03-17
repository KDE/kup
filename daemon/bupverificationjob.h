// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef BUPVERIFICATIONJOB_H
#define BUPVERIFICATIONJOB_H

#include "backupjob.h"

#include <KProcess>

class KupDaemon;

class BupVerificationJob : public BackupJob
{
    Q_OBJECT

public:
    BupVerificationJob(BackupPlan &pBackupPlan, const QString &pDestinationPath, const QString &pLogFilePath, KupDaemon *pKupDaemon);

protected slots:
    void performJob() override;
    void slotCheckingStarted();
    void slotCheckingDone(int pExitCode, QProcess::ExitStatus pExitStatus);

protected:
    KProcess mFsckProcess;
};

#endif // BUPVERIFICATIONJOB_H
