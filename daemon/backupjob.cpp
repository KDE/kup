// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "backupjob.h"
#include "bupjob.h"
#include "kupdaemon.h"
#include "rsyncjob.h"

#include <sys/resource.h>
#include <unistd.h>
#ifdef Q_OS_LINUX
#include <sys/syscall.h>
#endif

#include <KLocalizedString>
#include <QTimer>
#include <utility>

BackupJob::BackupJob(BackupPlan &pBackupPlan, QString pDestinationPath, QString pLogFilePath, KupDaemon *pKupDaemon)
    : mBackupPlan(pBackupPlan)
    , mDestinationPath(std::move(pDestinationPath))
    , mLogFilePath(std::move(pLogFilePath))
    , mKupDaemon(pKupDaemon)
{
    mLogFile.setFileName(mLogFilePath);
    mLogFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
    mLogStream.setDevice(&mLogFile);
}

void BackupJob::start()
{
    mKupDaemon->registerJob(this);
    QStringList lRemovedPaths;
    for (const QString &lPath : std::as_const(mBackupPlan.mPathsIncluded)) {
        if (!QFile::exists(lPath)) {
            lRemovedPaths << lPath;
        }
    }
    if (!lRemovedPaths.isEmpty()) {
        jobFinishedError(ErrorSourcesConfig,
                         xi18ncp("@info notification",
                                 "One source folder no longer exists. Please open settings and confirm what to include in backup.<nl/>"
                                 "<filename>%2</filename>",
                                 "%1 source folders no longer exist. Please open settings and confirm what to include in backup.<nl/>"
                                 "<filename>%2</filename>",
                                 lRemovedPaths.length(),
                                 lRemovedPaths.join(QChar('\n'))));
        return;
    }
    QTimer::singleShot(0, this, &BackupJob::performJob);
}

void BackupJob::makeNice(int pPid)
{
#ifdef Q_OS_LINUX
    // See linux documentation Documentation/block/ioprio.txt for details of the syscall
    syscall(SYS_ioprio_set, 1, pPid, 3 << 13 | 7);
#endif
    setpriority(PRIO_PROCESS, static_cast<uint>(pPid), 19);
}

QString BackupJob::quoteArgs(const QStringList &pCommand)
{
    QString lResult;
    bool lFirst = true;
    foreach (const QString &lArg, pCommand) {
        if (lFirst) {
            lResult.append(lArg);
            lFirst = false;
        } else {
            lResult.append(QStringLiteral(" \""));
            lResult.append(lArg);
            lResult.append(QStringLiteral("\""));
        }
    }
    return lResult;
}

void BackupJob::jobFinishedSuccess()
{
    // unregistring a job will normally show a UI notification that it the job was completed
    // setting the error code to indicate that the user canceled the job makes the UI not show
    // any notification. We want that since we want to trigger our own notification which has
    // more buttons and stuff.
    setError(KilledJobError);
    mKupDaemon->unregisterJob(this);

    // The error code is still used by our internal logic, for triggering our own notification.
    // So make sure to set it correctly.
    setError(NoError);
    emitResult();
}

void BackupJob::jobFinishedError(BackupJob::ErrorCodes pErrorCode, const QString &pErrorText)
{
    // if job has already set the error that it was killed by the user then ignore any fault
    // we get here as that fault is surely about the process exit code was not zero.
    // And we don't want to report about that (with our notification) in this case.
    bool lWasKilled = (error() == KilledJobError);

    setError(KilledJobError);
    mKupDaemon->unregisterJob(this);
    if (!lWasKilled) {
        setError(pErrorCode);
        setErrorText(pErrorText);
    }
    emitResult();
}
