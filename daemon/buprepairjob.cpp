// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "buprepairjob.h"

#include <QThread>

#include <KLocalizedString>

BupRepairJob::BupRepairJob(BackupPlan &pBackupPlan, const QString &pDestinationPath, const QString &pLogFilePath, KupDaemon *pKupDaemon)
    : BackupJob(pBackupPlan, pDestinationPath, pLogFilePath, pKupDaemon)
{
    mFsckProcess.setOutputChannelMode(KProcess::SeparateChannels);
}

void BupRepairJob::performJob()
{
    KProcess lPar2Process;
    lPar2Process.setOutputChannelMode(KProcess::SeparateChannels);
    lPar2Process << QStringLiteral("bup") << QStringLiteral("fsck") << QStringLiteral("--par2-ok");
    int lExitCode = lPar2Process.execute();
    if (lExitCode < 0) {
        jobFinishedError(ErrorWithoutLog,
                         xi18nc("@info notification",
                                "The <application>bup</application> program is needed but could not be found, "
                                "maybe it is not installed?"));
        return;
    }
    if (mBackupPlan.mGenerateRecoveryInfo && lExitCode != 0) {
        jobFinishedError(ErrorWithoutLog,
                         xi18nc("@info notification",
                                "The <application>par2</application> program is needed but could not be found, "
                                "maybe it is not installed?"));
        return;
    }

    mLogStream << QStringLiteral("Kup is starting bup repair job at ") << QLocale().toString(QDateTime::currentDateTime()) << Qt::endl << Qt::endl;

    mFsckProcess << QStringLiteral("bup");
    mFsckProcess << QStringLiteral("-d") << mDestinationPath;
    mFsckProcess << QStringLiteral("fsck") << QStringLiteral("-r");
    mFsckProcess << QStringLiteral("-j") << QString::number(qMin(4, QThread::idealThreadCount()));

    connect(&mFsckProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &BupRepairJob::slotRepairDone);
    connect(&mFsckProcess, &KProcess::started, this, &BupRepairJob::slotRepairStarted);
    mLogStream << mFsckProcess.program().join(QStringLiteral(" ")) << Qt::endl;
    mFsckProcess.start();
}

void BupRepairJob::slotRepairStarted()
{
    makeNice(mFsckProcess.processId());
}

void BupRepairJob::slotRepairDone(int pExitCode, QProcess::ExitStatus pExitStatus)
{
    QString lErrors = QString::fromUtf8(mFsckProcess.readAllStandardError());
    if (!lErrors.isEmpty()) {
        mLogStream << lErrors << Qt::endl;
    }
    mLogStream << "Exit code: " << pExitCode << Qt::endl;
    if (pExitStatus != QProcess::NormalExit) {
        mLogStream << QStringLiteral(
            "Repair failed (the repair process crashed). Your backups could be "
            "corrupted! See above for details.")
                   << Qt::endl;
        jobFinishedError(ErrorWithLog,
                         xi18nc("@info notification",
                                "Backup repair failed. Your backups could be corrupted! "
                                "See log file for more details."));
    } else if (pExitCode == 100) {
        mLogStream << QStringLiteral("Repair succeeded. See above for details.") << Qt::endl;
        jobFinishedError(ErrorWithLog, xi18nc("@info notification", "Success! Backup repair worked. See log file for more details."));
    } else if (pExitCode == 0) {
        mLogStream << QStringLiteral(
            "Repair was not necessary. Your backups are fine. See "
            "above for details.")
                   << Qt::endl;
        jobFinishedError(ErrorWithLog,
                         xi18nc("@info notification",
                                "Backup repair was not necessary. Your backups are not corrupted. "
                                "See log file for more details."));
    } else {
        mLogStream << QStringLiteral(
            "Repair failed. Your backups could still be "
            "corrupted! See above for details.")
                   << Qt::endl;
        jobFinishedError(ErrorWithLog,
                         xi18nc("@info notification",
                                "Backup repair failed. Your backups could still be corrupted! "
                                "See log file for more details."));
    }
}
