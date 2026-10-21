// SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "restorejob.h"

#include <KAuth/Action>
#include <KAuth/ExecuteJob>
#include <KJob>

#include <QGuiApplication>
#include <QWindow>

using namespace Qt::StringLiterals;

RestoreJob::RestoreJob(QObject *pParent)
    : KCompositeJob(pParent)
{
    qCritical() << "RestoreJob ctor";
}

bool RestoreJob::wantsRestore() const
{
    return mWantsRestore;
}

void RestoreJob::setWantsRestore(bool pWants)
{
    if (pWants == mWantsRestore) {
        return;
    }
    mWantsRestore = pWants;
    Q_EMIT wantsRestoreChanged();
}

void RestoreJob::setBackup(QString pFsPath, QString pName, QString pRevision, QString pUsername)
{
    mBackupFsPath = pFsPath;
    mBackupName = pName;
    mBackupUsername = pUsername;
    mBackupRevision = pRevision;
}

void RestoreJob::start()
{
    qCritical() << "RestoreJob::start";
    if (!mWantsRestore) {
        emitResult();
        return;
    }

    QList<QWindow *> lTopLevelWindows = QGuiApplication::topLevelWindows();
    QWindow *lWindow = lTopLevelWindows.isEmpty() ? nullptr : lTopLevelWindows.first();

    KAuth::Action lAction("org.kde.plasmasetup.restorebackup.restorebackup"_L1);
    lAction.setParentWindow(lWindow);
    lAction.setHelperId("org.kde.plasmasetup.restorebackup"_L1);
    lAction.setArguments({
        {"username"_L1, mBackupUsername},
        {"backupDir"_L1, mBackupFsPath},
        {"backupName"_L1, mBackupName},
        {"backupRevision"_L1, mBackupRevision},
    });
    KAuth::ExecuteJob *lJob = lAction.execute();

    addSubjob(lJob);
    lJob->start();
}

void RestoreJob::slotResult(KJob *pJob)
{
    KCompositeJob::slotResult(pJob);
    emitResult();
}
