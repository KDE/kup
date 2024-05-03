// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "bupverificationjob.h"

#include <QThread>

#include <KLocalizedString>

BupVerificationJob::BupVerificationJob(BackupPlan &pBackupPlan, const QString &pDestinationPath, const QString &pLogFilePath, KupDaemon *pKupDaemon)
    : BackupJob(pBackupPlan, pDestinationPath, pLogFilePath, pKupDaemon)
{
    mFsckProcess.setOutputChannelMode(KProcess::SeparateChannels);
}

void BupVerificationJob::performJob()
{
    KProcess lVersionProcess;
    lVersionProcess.setOutputChannelMode(KProcess::SeparateChannels);
    lVersionProcess << QStringLiteral("bup") << QStringLiteral("version");
    if (lVersionProcess.execute() < 0) {
        jobFinishedError(ErrorWithoutLog,
                         xi18nc("@info notification",
                                "The <application>bup</application> program is needed but could not be found, "
                                "maybe it is not installed?"));
        return;
    }

    mLogStream << QStringLiteral("Kup is starting bup verification job at ") << QLocale().toString(QDateTime::currentDateTime()) << Qt::endl << Qt::endl;

    mFsckProcess << QStringLiteral("bup");
    mFsckProcess << QStringLiteral("-d") << mDestinationPath;
    mFsckProcess << QStringLiteral("fsck") << QStringLiteral("--quick");
    mFsckProcess << QStringLiteral("-j") << QString::number(qMin(4, QThread::idealThreadCount()));

    connect(&mFsckProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &BupVerificationJob::slotCheckingDone);
    connect(&mFsckProcess, &KProcess::started, this, &BupVerificationJob::slotCheckingStarted);
    mLogStream << mFsckProcess.program().join(QStringLiteral(" ")) << Qt::endl;
    mFsckProcess.start();
}

void BupVerificationJob::slotCheckingStarted()
{
    makeNice(mFsckProcess.processId());
}

void BupVerificationJob::slotCheckingDone(int pExitCode, QProcess::ExitStatus pExitStatus)
{
    QString lErrors = QString::fromUtf8(mFsckProcess.readAllStandardError());
    if (!lErrors.isEmpty()) {
        mLogStream << lErrors << Qt::endl;
    }
    mLogStream << "Exit code: " << pExitCode << Qt::endl;
    if (pExitStatus != QProcess::NormalExit) {
        mLogStream << QStringLiteral(
            "Integrity check failed (the process crashed). Your backups could be "
            "corrupted! See above for details.")
                   << Qt::endl;
        if (mBackupPlan.mGenerateRecoveryInfo) {
            jobFinishedError(ErrorSuggestRepair,
                             xi18nc("@info notification",
                                    "Failed backup integrity check. Your backups could be corrupted! "
                                    "See log file for more details. Do you want to try repairing the backup files?"));
        } else {
            jobFinishedError(ErrorWithLog,
                             xi18nc("@info notification",
                                    "Failed backup integrity check. Your backups are corrupted! "
                                    "See log file for more details."));
        }
    } else if (pExitCode == 0) {
        mLogStream << QStringLiteral(
            "Backup integrity test was successful. "
            "Your backups are fine. See above for details.")
                   << Qt::endl;
        jobFinishedError(ErrorWithLog,
                         xi18nc("@info notification",
                                "Backup integrity test was successful. "
                                "Your backups are fine."));
    } else {
        mLogStream << QStringLiteral(
            "Integrity check failed. Your backups are "
            "corrupted! See above for details.")
                   << Qt::endl;
        if (mBackupPlan.mGenerateRecoveryInfo) {
            jobFinishedError(ErrorSuggestRepair,
                             xi18nc("@info notification",
                                    "Failed backup integrity check. Your backups are corrupted! "
                                    "See log file for more details. Do you want to try repairing the backup files?"));

        } else {
            jobFinishedError(ErrorWithLog,
                             xi18nc("@info notification",
                                    "Failed backup integrity check. Your backups are corrupted! "
                                    "See log file for more details."));
        }
    }
}
