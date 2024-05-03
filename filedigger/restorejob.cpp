// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "restorejob.h"

#include <KLocalizedString>
#include <QDir>
#include <utility>

#include <sys/resource.h>
#include <unistd.h>
#ifdef Q_OS_LINUX
#include <sys/syscall.h>
#endif

RestoreJob::RestoreJob(QString pRepositoryPath,
                       QString pSourcePath,
                       QString pRestorationPath,
                       int pTotalDirCount,
                       qint64 pTotalFileSize,
                       const QHash<QString, qint64> &pFileSizes)
    : mRepositoryPath(std::move(pRepositoryPath))
    , mSourcePath(std::move(pSourcePath))
    , mRestorationPath(std::move(pRestorationPath))
    , mTotalDirCount(pTotalDirCount)
    , mTotalFileSize(pTotalFileSize)
    , mFileSizes(pFileSizes)
{
    setCapabilities(Killable);
    mRestoreProcess.setOutputChannelMode(KProcess::SeparateChannels);
    int lOffset = mSourcePath.endsWith(QDir::separator()) ? -2 : -1;
    mSourceFileName = mSourcePath.section(QDir::separator(), lOffset, lOffset);
}

void RestoreJob::start()
{
    setTotalAmount(Bytes, mTotalFileSize);
    setProcessedAmount(Bytes, 0);
    setTotalAmount(Files, static_cast<quint64>(mFileSizes.count()));
    setProcessedAmount(Files, 0);
    setTotalAmount(Directories, static_cast<quint64>(mTotalDirCount));
    setProcessedAmount(Directories, 0);
    setPercent(0);
    mRestoreProcess << QStringLiteral("bup");
    mRestoreProcess << QStringLiteral("-d") << mRepositoryPath;
    mRestoreProcess << QStringLiteral("restore") << QStringLiteral("-vv");
    mRestoreProcess << QStringLiteral("-C") << mRestorationPath;
    mRestoreProcess << mSourcePath;
    connect(&mRestoreProcess, &KProcess::started, this, &RestoreJob::slotRestoringStarted);
    connect(&mRestoreProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &RestoreJob::slotRestoringDone);
    mRestoreProcess.start();
    mTimerId = startTimer(100);
}

void RestoreJob::slotRestoringStarted()
{
    makeNice(mRestoreProcess.processId());
}

void RestoreJob::timerEvent(QTimerEvent *pTimerEvent)
{
    Q_UNUSED(pTimerEvent)
    quint64 lProcessedDirectories = processedAmount(Directories);
    quint64 lProcessedFiles = processedAmount(Files);
    quint64 lProcessedBytes = processedAmount(Bytes);
    bool lDirWasUpdated = false;
    bool lFileWasUpdated = false;
    QString lLastFileName;

    while (mRestoreProcess.canReadLine()) {
        QString lFileName = QString::fromLocal8Bit(mRestoreProcess.readLine()).trimmed();
        if (lFileName.size() == 0) {
            break;
        }
        if (lFileName.endsWith(QLatin1Char('/'))) { // it's a directory
            lProcessedDirectories++;
            lDirWasUpdated = true;
        } else {
            if (mSourcePath.endsWith(QDir::separator())) {
                lFileName.prepend(QDir::separator());
                lFileName.prepend(mSourceFileName);
            }
            lProcessedBytes += mFileSizes.value(lFileName);
            lProcessedFiles++;
            lLastFileName = lFileName;
            lFileWasUpdated = true;
        }
    }
    if (lDirWasUpdated) {
        setProcessedAmount(Directories, lProcessedDirectories);
    }
    if (lFileWasUpdated) {
        emit description(this, xi18nc("progress report, current operation", "Restoring"), qMakePair(xi18nc("progress report, label", "File"), lLastFileName));
        setProcessedAmount(Files, lProcessedFiles);
        setProcessedAmount(Bytes, lProcessedBytes); // this will also call emitPercent()
    }
}

void RestoreJob::slotRestoringDone(int pExitCode, QProcess::ExitStatus pExitStatus)
{
    killTimer(mTimerId);
    if (pExitStatus != QProcess::NormalExit || pExitCode != 0) {
        setError(1);
        setErrorText(QString::fromUtf8(mRestoreProcess.readAllStandardError()));
    }
    emitResult();
}

void RestoreJob::makeNice(int pPid)
{
#ifdef Q_OS_LINUX
    // See linux documentation Documentation/block/ioprio.txt for details of the syscall
    syscall(SYS_ioprio_set, 1, pPid, 3 << 13 | 7);
#endif
    setpriority(PRIO_PROCESS, static_cast<uint>(pPid), 19);
}
