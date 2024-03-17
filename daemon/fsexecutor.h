// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef FSEXECUTOR_H
#define FSEXECUTOR_H

#include "planexecutor.h"

#include <QThread>

class BackupPlan;
class KDirWatch;
class QTimer;

// KDirWatch (well, inotify) does not detect when something gets mounted on a watched directory.
// work around this problem by monitoring the mounts of the system in a separate thread.
class MountWatcher : public QThread
{
    Q_OBJECT

signals:
    void mountsChanged();

protected:
    void run() override;
};

// Plan executor that stores the backup to a path in the local
// filesystem, uses KDirWatch to monitor for when the folder
// becomes available/unavailable. Can be used for external
// drives or networked filesystems if you always mount it at
// the same mountpoint.
class FSExecutor : public PlanExecutor
{
    Q_OBJECT

public:
    FSExecutor(BackupPlan *pPlan, KupDaemon *pKupDaemon);
    ~FSExecutor() override;

public slots:
    void checkStatus() override;

protected slots:
    void checkMountPoints();

protected:
    QString mWatchedParentDir;
    KDirWatch *mDirWatch;
    MountWatcher mMountWatcher;
};

#endif // FSEXECUTOR_H
