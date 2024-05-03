// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "rsyncjob.h"
#include "kuputils.h"

#include <csignal>

#include <QDir>
#include <QRegularExpression>
#include <QTextStream>

#include <KLocalizedString>

RsyncJob::RsyncJob(BackupPlan &pBackupPlan, const QString &pDestinationPath, const QString &pLogFilePath, KupDaemon *pKupDaemon)
    : BackupJob(pBackupPlan, pDestinationPath, pLogFilePath, pKupDaemon)
{
    mRsyncProcess.setOutputChannelMode(KProcess::SeparateChannels);
    setCapabilities(KJob::Suspendable | KJob::Killable);
}

void RsyncJob::performJob()
{
    KProcess lVersionProcess;
    lVersionProcess.setOutputChannelMode(KProcess::SeparateChannels);
    lVersionProcess << QStringLiteral("rsync") << QStringLiteral("--version");
    if (lVersionProcess.execute() < 0) {
        jobFinishedError(ErrorWithoutLog,
                         xi18nc("@info notification",
                                "The <application>rsync</application> program is needed but "
                                "could not be found, maybe it is not installed?"));
        return;
    }

    // Remove this and the performMigration method when it is likely that all users of pre 0.8 kup have now started using post 0.8.
    if (mBackupPlan.mBackupVersion < 1 && mBackupPlan.mLastCompleteBackup.isValid() && mBackupPlan.mPathsIncluded.length() == 1) {
        mLogStream << QStringLiteral("Migrating saved files to new location, after update to version 0.8 of Kup.") << Qt::endl;
        if (!performMigration()) {
            mLogStream << QStringLiteral("Migration failed. Continuing backup save regardless, may result in files stored twice.") << Qt::endl;
        }
    }
    mBackupPlan.mBackupVersion = 1;
    mBackupPlan.save();

    mLogStream << QStringLiteral("Kup is starting rsync backup job at ") << QLocale().toString(QDateTime::currentDateTime()) << Qt::endl;

    emit description(this, i18n("Checking what to copy"));
    mRsyncProcess << QStringLiteral("rsync") << QStringLiteral("-avX") << QStringLiteral("--delete-excluded") << QStringLiteral("--delete-before")
                  << QStringLiteral("--info=progress2");

    QStringList lIncludeNames;
    foreach (const QString &lInclude, mBackupPlan.mPathsIncluded) {
        lIncludeNames << lastPartOfPath(lInclude);
    }
    if (lIncludeNames.removeDuplicates() > 0) {
        // There would be a naming conflict in the destination folder, instead use full paths.
        mRsyncProcess << QStringLiteral("-R");
        foreach (const QString &lExclude, mBackupPlan.mPathsExcluded) {
            mRsyncProcess << QStringLiteral("--exclude") << lExclude;
        }
    } else {
        // when NOT using -R, need to then strip parent paths from excludes, everything above the
        // include. Leave the leading slash!
        foreach (QString lExclude, mBackupPlan.mPathsExcluded) {
            for (int i = 0; i < mBackupPlan.mPathsIncluded.length(); ++i) {
                const QString &lInclude = mBackupPlan.mPathsIncluded.at(i);
                QString lIncludeWithSlash = lInclude;
                ensureTrailingSlash(lIncludeWithSlash);
                if (lExclude.startsWith(lIncludeWithSlash)) {
                    lExclude.remove(0, lInclude.length() - lIncludeNames.at(i).length() - 1);
                    break;
                }
            }
            mRsyncProcess << QStringLiteral("--exclude") << lExclude;
        }
    }
    QString lExcludesPath = mBackupPlan.absoluteExcludesFilePath();
    if (mBackupPlan.mExcludePatterns && QFileInfo::exists(lExcludesPath)) {
        mRsyncProcess << QStringLiteral("--exclude-from") << lExcludesPath;
    }
    mRsyncProcess << mBackupPlan.mPathsIncluded;
    mRsyncProcess << mDestinationPath;

    connect(&mRsyncProcess, &KProcess::started, this, &RsyncJob::slotRsyncStarted);
    connect(&mRsyncProcess, &KProcess::readyReadStandardOutput, this, &RsyncJob::slotReadRsyncOutput);
    connect(&mRsyncProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &RsyncJob::slotRsyncFinished);
    mLogStream << quoteArgs(mRsyncProcess.program()) << Qt::endl;
    mRsyncProcess.start();
    mInfoRateLimiter.start();
}

void RsyncJob::slotRsyncStarted()
{
    makeNice(mRsyncProcess.processId());
}

void RsyncJob::slotRsyncFinished(int pExitCode, QProcess::ExitStatus pExitStatus)
{
    QString lErrors = QString::fromUtf8(mRsyncProcess.readAllStandardError());
    if (!lErrors.isEmpty()) {
        mLogStream << lErrors << Qt::endl;
    }
    mLogStream << "Exit code: " << pExitCode << Qt::endl;
    // exit code 24 means source files disappeared during copying. No reason to worry about that.
    if (pExitStatus != QProcess::NormalExit || (pExitCode != 0 && pExitCode != 24)) {
        mLogStream << QStringLiteral("Kup did not successfully complete the rsync backup job.") << Qt::endl;
        jobFinishedError(ErrorWithLog,
                         xi18nc("@info notification",
                                "Failed to save backup. "
                                "See log file for more details."));
    } else {
        mLogStream << QStringLiteral("Kup successfully completed the rsync backup job at ") << QLocale().toString(QDateTime::currentDateTime()) << Qt::endl;
        jobFinishedSuccess();
    }
}

void RsyncJob::slotReadRsyncOutput()
{
    bool lValidInfo = false;
    bool lValidFileName = false;
    QString lFileName;
    ulong lPercent{};
    qulonglong lTransfered{};
    double lSpeed{};
    QChar lUnit;
    static QRegularExpression lProgressInfoExp(QStringLiteral("^\\s+([\\d,\\.]+)\\s+(\\d+)%\\s+(\\d*[,\\.]\\d+)(\\S)"));
    // very ugly and rough indication that this is a file path... what else to do..
    static QRegularExpression lNotFileNameExp(QStringLiteral("^(building file list|done$|deleting \\S+|.+/$|$)"));
    QString lLine;

    QTextStream lStream(mRsyncProcess.readAllStandardOutput());
    while (lStream.readLineInto(&lLine, 500)) {
        QRegularExpressionMatch lMatch = lProgressInfoExp.match(lLine);
        if (lMatch.hasMatch()) {
            lValidInfo = true;
            lTransfered = lMatch.captured(1).remove(',').remove('.').toULongLong();
            lPercent = qMax(lMatch.captured(2).toULong(), 1UL);
            lSpeed = QLocale().toDouble(lMatch.captured(3));
            lUnit = lMatch.captured(4).at(0);
        } else {
            lMatch = lNotFileNameExp.match(lLine);
            if (!lMatch.hasMatch()) {
                lValidFileName = true;
                lFileName = lLine;
            }
        }
    }
    if (mInfoRateLimiter.hasExpired(200)) {
        if (lValidInfo) {
            setPercent(lPercent);
            if (lUnit == 'k') {
                lSpeed *= 1e3;
            } else if (lUnit == 'M') {
                lSpeed *= 1e6;
            } else if (lUnit == 'G') {
                lSpeed *= 1e9;
            }
            emitSpeed(static_cast<ulong>(lSpeed));
            if (lPercent > 5) { // the rounding to integer percent gives big error with small percentages
                setProcessedAmount(KJob::Bytes, lTransfered);
                setTotalAmount(KJob::Bytes, lTransfered * 100 / lPercent);
            }
        }
        if (lValidFileName) {
            emit description(this, i18n("Saving backup"), qMakePair(i18nc("Label for file currently being copied", "File"), lFileName));
        }
        mInfoRateLimiter.start();
    }
}

bool RsyncJob::doKill()
{
    setError(KilledJobError);
    if (0 == ::kill(mRsyncProcess.processId(), SIGINT)) {
        return mRsyncProcess.waitForFinished();
    }
    return false;
}

bool RsyncJob::doSuspend()
{
    return 0 == ::kill(mRsyncProcess.processId(), SIGSTOP);
}

bool RsyncJob::doResume()
{
    return 0 == ::kill(mRsyncProcess.processId(), SIGCONT);
}

// This migration moves files from being stored directly in destination folder, to
// being stored in a subfolder of the destination. The subfolder is named same as the
// source folder. This migration will only be done if there is exactly one source folder.
bool RsyncJob::performMigration()
{
    QString lSourceDirName = lastPartOfPath(mBackupPlan.mPathsIncluded.first()); // only one included
    QDir lDestDir = QDir(mDestinationPath);
    mLogStream << QStringLiteral("Creating directory named ") << lSourceDirName << " inside of " << mDestinationPath << Qt::endl;
    if (!lDestDir.mkdir(lSourceDirName)) {
        mLogStream << QStringLiteral("Failed to create directory, aborting migration.") << Qt::endl;
        return false;
    }
    foreach (const QString &lContent, lDestDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
        if (lContent != lSourceDirName) {
            QString lDest = lSourceDirName + QLatin1Char('/') + lContent;
            mLogStream << QStringLiteral("Renaming ") << lContent << " to " << lDest << Qt::endl;
            if (!lDestDir.rename(lContent, lDest)) {
                mLogStream << QStringLiteral("Failed to rename, aborting migration.") << Qt::endl;
                return false;
            }
        }
    }
    mLogStream << QStringLiteral("File migration completed.") << Qt::endl;
    return true;
}
