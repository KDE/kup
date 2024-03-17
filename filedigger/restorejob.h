// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef RESTOREJOB_H
#define RESTOREJOB_H

#include "versionlistmodel.h"

#include <KJob>
#include <KProcess>

class RestoreJob : public KJob
{
    Q_OBJECT
public:
    explicit RestoreJob(QString pRepositoryPath,
                        QString pSourcePath,
                        QString pRestorationPath,
                        int pTotalDirCount,
                        qint64 pTotalFileSize,
                        const QHash<QString, qint64> &pFileSizes);
    void start() override;

protected slots:
    void slotRestoringStarted();
    void slotRestoringDone(int pExitCode, QProcess::ExitStatus pExitStatus);

protected:
    void timerEvent(QTimerEvent *pTimerEvent) override;
    static void makeNice(int pPid);
    void moveFolder();

    KProcess mRestoreProcess;
    QString mRepositoryPath;
    QString mSourcePath;
    QString mRestorationPath;
    QString mSourceFileName;
    int mTotalDirCount;
    qint64 mTotalFileSize;
    const QHash<QString, qint64> &mFileSizes;
    int mTimerId{};
};

#endif // RESTOREJOB_H
