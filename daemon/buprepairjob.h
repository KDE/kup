// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef BUPREPAIRJOB_H
#define BUPREPAIRJOB_H

#include "backupjob.h"

#include <KProcess>

class KupDaemon;

class BupRepairJob : public BackupJob
{
    Q_OBJECT

public:
    BupRepairJob(BackupPlan &pBackupPlan, const QString &pDestinationPath, const QString &pLogFilePath, KupDaemon *pKupDaemon);

protected slots:
    void performJob() override;
    void slotRepairStarted();
    void slotRepairDone(int pExitCode, QProcess::ExitStatus pExitStatus);

protected:
    KProcess mFsckProcess;
};

#endif // BUPREPAIRJOB_H
