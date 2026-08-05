// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "backupjob.h"
#include "../settings/dynamicexclusions.h"
#include "../settings/filescanner.h"
#include "bupjob.h"
#include "kupdaemon.h"
#include "kupdaemon_debug.h"
#include "rsyncjob.h"

#include <sys/resource.h>
#include <unistd.h>
#ifdef Q_OS_LINUX
#include <sys/syscall.h>
#endif

#include <KLocalizedString>
#include <QTimer>
#include <utility>

using namespace Qt::StringLiterals;

BackupJob::BackupJob(BackupPlan &pBackupPlan, QString pDestinationPath, QString pLogFilePath, KupDaemon *pKupDaemon)
    : mBackupPlan(pBackupPlan)
    , mDestinationPath(std::move(pDestinationPath))
    , mLogFilePath(std::move(pLogFilePath))
    , mKupDaemon(pKupDaemon)
{
    mLogFile.setFileName(mLogFilePath);
    mLogFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
    mLogStream.setDevice(&mLogFile);

    // Magic property that tells the job tracker the destination of this job.
    setProperty("destUrl", "file://"_L1 + mDestinationPath);
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

    DynamicExclusions lDynExclusions;
    lDynExclusions.setFromPlan(mBackupPlan);

    FileScanner *lFileScanner = new FileScanner(this);
    lFileScanner->setExclusionRegExps(mBackupPlan.excludePatterns());

    connect(lFileScanner, &FileScanner::scanFinished, this, [this, lFileScanner]() {
        if (!lFileScanner->unreadableFiles().isEmpty() || !lFileScanner->unreadableFolders().isEmpty()) {
            const QSet<QString> lAllUnreadablesSet = lFileScanner->unreadableFiles() + lFileScanner->unreadableFolders();
            const QStringList lAllUnreadables(lAllUnreadablesSet.constBegin(), lAllUnreadablesSet.constEnd());

            QString lErrorText;
            if (lAllUnreadables.size() == lFileScanner->unreadableFiles().size()) {
                lErrorText = i18ncp("@info notification",
                                    "One source file is unreadable. Please check its permissions, or exclude it in settings.",
                                    "%1 source files are unreadable. Please check their permissions, or exclude them in settings.",
                                    lAllUnreadables.size());
            } else if (lAllUnreadables.size() == lFileScanner->unreadableFolders().size()) {
                lErrorText = i18ncp("@info notification",
                                    "One source folder is unreadable. Please check its permissions, or exclude it in settings.",
                                    "%1 source folders are unreadable. Please check their permissions, or exclude them in settings.",
                                    lAllUnreadables.size());
            } else {
                // we need the pluralization form here too, even though we know that there are >1 such files
                // because different languages might have more complex rules for numbers than 1/not-1
                lErrorText = i18ncp("@info notification",
                                    "One source file or folder is unreadable. Please check its permissions, or exclude it in settings.",
                                    "%1 source files or folders are unreadable. Please check their permissions, or exclude them in settings.",
                                    lAllUnreadables.size());
            }

            if (lAllUnreadables.size() <= 3) {
                lErrorText =
                    xi18nc("@info notification %1 is one of the 'N source file(s)/folder(s) is/are unreadable' messages above, %2 is a list of filenames",
                           "<para>%1<nl/><bcode>%2</bcode></para>",
                           lErrorText,
                           lAllUnreadables.join(QString("\n")));
                jobFinishedError(ErrorUnreadable, lErrorText);
            } else {
                // if there are too many entries to display comfortably in the notification itself
                // then we write them out to the log and prompt the user to open that
                for (const QString &lUnreadable : lAllUnreadables) {
                    mLogStream << lUnreadable << QChar('\n');
                }
                jobFinishedError(ErrorUnreadableWithLog, lErrorText);
            }

            return;
        }

        QTimer::singleShot(0, this, &BackupJob::performJob);
    });

    const QStringList lAllExclusions = mBackupPlan.mPathsExcluded + lDynExclusions.pathsExcluded(mBackupPlan.mPathsIncluded);
    for (const QString &lExclude : lAllExclusions) {
        lFileScanner->excludePath(lExclude);
    }

    QHash<QString, bool> lPathScansPending;
    for (const QString &lPath : std::as_const(mBackupPlan.mPathsIncluded)) {
        lFileScanner->includePath(lPath);
    }
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
