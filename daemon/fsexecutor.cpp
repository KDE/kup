// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "fsexecutor.h"
#include "backupplan.h"

#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QTimer>

#include <KDirWatch>

#include <fcntl.h>
#include <sys/select.h>
#include <sys/stat.h>

namespace
{

// very light check if a directory exists that works on automounts where QDir::exists fails
bool checkDirExists(const QDir &dir)
{
    struct stat s;
    return stat(dir.absolutePath().toLocal8Bit().data(), &s) == 0 && S_ISDIR(s.st_mode);
}
}

FSExecutor::FSExecutor(BackupPlan *pPlan, KupDaemon *pKupDaemon)
    : PlanExecutor(pPlan, pKupDaemon)
{
    mDestinationPath = QDir::cleanPath(mPlan->mFilesystemDestinationPath.toLocalFile());
    mDirWatch = new KDirWatch(this);
    connect(mDirWatch, SIGNAL(deleted(QString)), SLOT(checkStatus()));
    mMountWatcher.start();
}

FSExecutor::~FSExecutor()
{
    mMountWatcher.terminate();
    mMountWatcher.wait();
}

void FSExecutor::checkStatus()
{
    static bool lComingBackLater = false;
    if (!mWatchedParentDir.isEmpty() && !lComingBackLater) {
        // came here because something happened to a parent folder,
        // come back in a few seconds, give a new mount some time before checking
        // status of destination folder
        QTimer::singleShot(5000, this, SLOT(checkStatus()));
        lComingBackLater = true;
        return;
    }
    lComingBackLater = false;

    QDir lDir(mDestinationPath);
    if (!lDir.exists()) {
        // Destination doesn't exist, find nearest existing parent folder and
        // watch that for dirty or deleted
        if (mDirWatch->contains(mDestinationPath)) {
            mDirWatch->removeDir(mDestinationPath);
        }

        QString lExisting = mDestinationPath;
        do {
            lExisting += QStringLiteral("/..");
            lDir = QDir(QDir::cleanPath(lExisting));
        } while (!checkDirExists(lDir));
        lExisting = lDir.canonicalPath();

        if (lExisting != mWatchedParentDir) { // new parent to watch
            if (!mWatchedParentDir.isEmpty()) { // were already watching a parent
                mDirWatch->removeDir(mWatchedParentDir);
            } else { // start watching a parent
                connect(mDirWatch, SIGNAL(dirty(QString)), SLOT(checkStatus()));
                connect(&mMountWatcher, SIGNAL(mountsChanged()), SLOT(checkMountPoints()), Qt::QueuedConnection);
            }
            mWatchedParentDir = lExisting;
            mDirWatch->addDir(mWatchedParentDir);
        }
        if (mState != NOT_AVAILABLE) {
            enterNotAvailableState();
        }
    } else {
        // Destination exists... only watch for delete
        if (!mWatchedParentDir.isEmpty()) {
            disconnect(mDirWatch, SIGNAL(dirty(QString)), this, SLOT(checkStatus()));
            disconnect(&mMountWatcher, SIGNAL(mountsChanged()), this, SLOT(checkMountPoints()));
            mDirWatch->removeDir(mWatchedParentDir);
            mWatchedParentDir.clear();
        }
        mDirWatch->addDir(mDestinationPath);

        QFileInfo lInfo(mDestinationPath);
        if (lInfo.isWritable() && mState == NOT_AVAILABLE) {
            enterAvailableState();
        } else if (!lInfo.isWritable() && mState != NOT_AVAILABLE) {
            enterNotAvailableState();
        }
    }
}

void FSExecutor::checkMountPoints()
{
    QFile lMountsFile(QStringLiteral("/proc/mounts"));
    if (!lMountsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    // don't use atEnd() to detect when finished reading file, size of
    // this special file is 0 but still returns data when read.
    forever {
        QByteArray lLine = lMountsFile.readLine();
        if (lLine.isEmpty()) {
            break;
        }
        QTextStream lTextStream(lLine);
        QString lDevice, lMountPoint;
        lTextStream >> lDevice >> lMountPoint;
        if (lMountPoint == mWatchedParentDir) {
            checkStatus();
        }
    }
}

void MountWatcher::run()
{
    int lMountsFd = open("/proc/mounts", O_RDONLY);
    fd_set lFdSet;

    forever {
        FD_ZERO(&lFdSet);
        FD_SET(lMountsFd, &lFdSet);
        if (select(lMountsFd + 1, nullptr, nullptr, &lFdSet, nullptr) > 0) {
            emit mountsChanged();
        }
    }
}
