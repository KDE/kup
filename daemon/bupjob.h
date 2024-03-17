// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef BUPJOB_H
#define BUPJOB_H

#include "backupjob.h"

#include <KProcess>
#include <QElapsedTimer>
#include <QRegularExpression>

class KupDaemon;

class BupJob : public BackupJob
{
    Q_OBJECT

public:
    BupJob(BackupPlan &pBackupPlan, const QString &pDestinationPath, const QString &pLogFilePath, KupDaemon *pKupDaemon);

protected slots:
    void performJob() override;
    void slotCheckingStarted();
    void slotCheckingDone(int pExitCode, QProcess::ExitStatus pExitStatus);
    void startIndexing();
    void slotIndexingStarted();
    void slotIndexingDone(int pExitCode, QProcess::ExitStatus pExitStatus);
    void slotSavingStarted();
    void slotSavingDone(int pExitCode, QProcess::ExitStatus pExitStatus);
    void slotRecoveryInfoStarted();
    void slotRecoveryInfoDone(int pExitCode, QProcess::ExitStatus pExitStatus);
    void slotReadBupErrors();

protected:
    bool doSuspend() override;
    bool doResume() override;

    KProcess mFsckProcess;
    KProcess mIndexProcess;
    KProcess mSaveProcess;
    KProcess mPar2Process;
    QElapsedTimer mInfoRateLimiter;
    int mHarmlessErrorCount;
    bool mAllErrorsHarmless;
    QRegularExpression mLineBreaksRegExp;
    QRegularExpression mNonsenseRegExp;
    QRegularExpression mFileGoneRegExp;
    QRegularExpression mProgressRegExp;
    QRegularExpression mErrorCountRegExp;
    QRegularExpression mFileInfoRegExp;
};

#endif /*BUPJOB_H*/
