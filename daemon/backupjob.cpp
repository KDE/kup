// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "backupjob.h"
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
#include <QDir>
#include <QTimer>
#include <utility>

#ifdef HAVE_LIBBTRFSUTIL
#include <KMountPoint>

#include <btrfsutil.h>
#include <linux/btrfs.h>
#include <linux/magic.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#endif

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
    startElapsedTimer();
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

#ifdef HAVE_LIBBTRFSUTIL
    if (mBackupPlan.mBackupFromSnapshot) {
        // determine what paths we can take snapshots of...
        QSet<QString> lSubvolumes;
        for (const auto &lInclude : mBackupPlan.mPathsIncluded) {
            bool isOnBtrfs = false;
#if KIO_VERSION >= QT_VERSION_CHECK(6, 30, 0)
            KMountPoint::Ptr mountPoint = KMountPoint::currentMountPointForPath(lInclude);
            isOnBtrfs = mountPoint->mountType() == QStringLiteral("btrfs");
#else
            struct statfs sfs;
            if (!(statfs(CSTR(fsPath), &sfs) < 0)) {
                isOnBtrfs = (sfs.f_type == BTRFS_SUPER_MAGIC);
            }
#endif // KIO_VERSION
            if (!isOnBtrfs) {
                continue;
            }

            QDir lSubvolumeRoot;
            QFileInfo fileInfo(lInclude);
            if (fileInfo.isDir()) {
                lSubvolumeRoot = QDir(lInclude);
            } else {
                lSubvolumeRoot = QDir(QFileInfo(lInclude).absoluteDir());
            }

            struct btrfs_util_subvolume_info lInfo;
            enum btrfs_util_error lBtrfsErr;

            while ((lBtrfsErr = btrfs_util_subvolume_get_info(lSubvolumeRoot.absolutePath().toLocal8Bit().constData(), 0, &lInfo)) != 0) {
                if (lSubvolumeRoot.isRoot()) {
                    break;
                }
                bool ok = lSubvolumeRoot.cdUp();
                if (!ok) {
                    break;
                }
            }

            if (lBtrfsErr != 0 || !QFileInfo(lSubvolumeRoot.absolutePath()).isWritable()) {
                continue;
            }

            lSubvolumes << QDir::cleanPath(lSubvolumeRoot.absolutePath());
        }

        // take the snapshot
        for (const auto &lSubvolume : std::as_const(lSubvolumes)) {
            QSet<QString> lSubSubvolumes;
            lSubSubvolumes << lSubvolume;

            enum btrfs_util_error lBtrfsErr;
            struct btrfs_util_subvolume_iterator *lBtrfsIter;
            lBtrfsErr = btrfs_util_subvolume_iter_create(lSubvolume.toLocal8Bit().constData(), 0, 0, &lBtrfsIter);
            char *lIterPath;
            struct btrfs_util_subvolume_info lIterInfo;
            if (lBtrfsErr == 0) {
                while ((lBtrfsErr = btrfs_util_subvolume_iter_next_info(lBtrfsIter, &lIterPath, &lIterInfo)) == 0) {
                    QString path = QDir::cleanPath(lSubvolume + QStringLiteral("/") + QString::fromUtf8(lIterPath));
                    free(lIterPath);
                    if (!mBackupPlan.mExcludeSnapshots || QUuid::fromBytes(lIterInfo.uuid).isNull()) {
                        // non-null implies that this subvolume is a snapshot
                        lSubSubvolumes << path;
                    }
                }
                btrfs_util_subvolume_iter_destroy(lBtrfsIter);
            }

            for (const auto &lSubSubvolume : std::as_const(lSubSubvolumes)) {
                const QString lSnapshotDest = QDir(lSubSubvolume).absoluteFilePath(".kup-snapshot-temp");
                if (QDir(lSnapshotDest).exists()) {
                    qCritical() << lSnapshotDest << "already exists";
                    lBtrfsErr = btrfs_util_subvolume_delete(lSnapshotDest.toLocal8Bit().constData(), 0);
                    if (lBtrfsErr != 0) {
                        qCritical(KUPDAEMON()) << "could not delete" << lSnapshotDest << ":" << btrfs_util_strerror(lBtrfsErr);
                    }
                    continue;
                }
                lBtrfsErr = btrfs_util_subvolume_snapshot(lSubSubvolume.toLocal8Bit().constData(), lSnapshotDest.toLocal8Bit().constData(), 0, NULL, NULL);
                if (lBtrfsErr != 0) {
                    qCritical(KUPDAEMON()) << "could not snapshot" << lSubSubvolume << "to" << lSnapshotDest << ":" << btrfs_util_strerror(lBtrfsErr);
                    continue;
                }
                mSourceSnapshots.insert(lSubSubvolume, lSnapshotDest);
            }
        }
    }
#endif // HAVE_LIBBTRFSUTIL

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
#ifdef HAVE_LIBBTRFSUTIL
    cleanSourceSnapshots();
#endif
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
#ifdef HAVE_LIBBTRFSUTIL
    cleanSourceSnapshots();
#endif
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

#ifdef HAVE_LIBBTRFSUTIL
void BackupJob::cleanSourceSnapshots()
{
    for (const auto &lSnapshotDest : mSourceSnapshots.values()) {
        enum btrfs_util_error lBtrfsErr = btrfs_util_subvolume_delete(lSnapshotDest.toLocal8Bit().constData(), 0);
        if (lBtrfsErr != 0) {
            qCritical(KUPDAEMON()) << "Could not delete snapshot" << lSnapshotDest << btrfs_util_strerror(lBtrfsErr);
        }
    }
}
#endif
