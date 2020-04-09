// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL

#include "fsexecutor.h"
#include "backupplan.h"

#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QTextStream>

#include <KDirWatch>
#include <KDiskFreeSpaceInfo>
#include <KIO/DirectorySizeJob>
#include <KLocalizedString>
#include <KNotification>

#include <fcntl.h>

FSExecutor::FSExecutor(BackupPlan *pPlan, KupDaemon *pKupDaemon)
   :PlanExecutor(pPlan, pKupDaemon)
{
	mDestinationPath = QDir::cleanPath(mPlan->mFilesystemDestinationPath.toLocalFile());
	mDirWatch = new KDirWatch(this);
	connect(mDirWatch, SIGNAL(deleted(QString)), SLOT(checkStatus()));
	mMountWatcher.start();
}

FSExecutor::~FSExecutor() {
	mMountWatcher.terminate();
	mMountWatcher.wait();
}

void FSExecutor::checkStatus() {
	static bool lComingBackLater = false;
	if(!mWatchedParentDir.isEmpty() && !lComingBackLater) {
		// came here because something happened to a parent folder,
		// come back in a few seconds, give a new mount some time before checking
		// status of destination folder
		QTimer::singleShot(5000, this, SLOT(checkStatus()));
		lComingBackLater = true;
		return;
	}
	lComingBackLater = false;

	QDir lDir(mDestinationPath);
	if(!lDir.exists()) {
		// Destination doesn't exist, find nearest existing parent folder and
		// watch that for dirty or deleted
		if(mDirWatch->contains(mDestinationPath)) {
			mDirWatch->removeDir(mDestinationPath);
		}

		QString lExisting = mDestinationPath;
		do {
			lExisting += QStringLiteral("/..");
			lDir = QDir(QDir::cleanPath(lExisting));
		} while(!lDir.exists());
		lExisting = lDir.canonicalPath();

		if(lExisting != mWatchedParentDir) { // new parent to watch
			if(!mWatchedParentDir.isEmpty()) { // were already watching a parent
				mDirWatch->removeDir(mWatchedParentDir);
			} else { // start watching a parent
				connect(mDirWatch, SIGNAL(dirty(QString)), SLOT(checkStatus()));
				connect(&mMountWatcher, SIGNAL(mountsChanged()), SLOT(checkMountPoints()), Qt::QueuedConnection);
			}
			mWatchedParentDir = lExisting;
			mDirWatch->addDir(mWatchedParentDir);
		}
		if(mState != NOT_AVAILABLE) {
			enterNotAvailableState();
		}
	} else {
		// Destination exists... only watch for delete
		if(!mWatchedParentDir.isEmpty()) {
			disconnect(mDirWatch, SIGNAL(dirty(QString)), this, SLOT(checkStatus()));
			disconnect(&mMountWatcher, SIGNAL(mountsChanged()), this, SLOT(checkMountPoints()));
			mDirWatch->removeDir(mWatchedParentDir);
			mWatchedParentDir.clear();
		}
		mDirWatch->addDir(mDestinationPath);

		QFileInfo lInfo(mDestinationPath);
		if(lInfo.isWritable() && mState == NOT_AVAILABLE) {
			enterAvailableState();
		}else if(!lInfo.isWritable() && mState != NOT_AVAILABLE) {
			enterNotAvailableState();
		}
	}
}

void FSExecutor::startBackup() {
	BackupJob *lJob = createBackupJob();
	if(lJob == nullptr) {
		KNotification::event(KNotification::Error, xi18nc("@title:window", "Problem"),
		                     xi18nc("notification", "Invalid type of backup in configuration."));
		exitBackupRunningState(false);
		return;
	}
	connect(lJob, SIGNAL(result(KJob*)), SLOT(slotBackupDone(KJob*)));
	lJob->start();
}

void FSExecutor::slotBackupDone(KJob *pJob) {
	if(pJob->error()) {
		if(pJob->error() != KJob::KilledJobError) {
			notifyBackupFailed(pJob);
		}
		exitBackupRunningState(false);
	} else {
		notifyBackupSucceeded();
		mPlan->mLastCompleteBackup = QDateTime::currentDateTimeUtc();
		KDiskFreeSpaceInfo lSpaceInfo = KDiskFreeSpaceInfo::freeSpaceInfo(mDestinationPath);
		if(lSpaceInfo.isValid())
			mPlan->mLastAvailableSpace = static_cast<double>(lSpaceInfo.available());
		else
			mPlan->mLastAvailableSpace = -1.0; //unknown size

		KIO::DirectorySizeJob *lSizeJob = KIO::directorySize(QUrl::fromLocalFile(mDestinationPath));
		connect(lSizeJob, SIGNAL(result(KJob*)), SLOT(slotBackupSizeDone(KJob*)));
		lSizeJob->start();
	}
}

void FSExecutor::slotBackupSizeDone(KJob *pJob) {
	if(pJob->error()) {
		KNotification::event(KNotification::Error, xi18nc("@title:window", "Problem"), pJob->errorText());
		mPlan->mLastBackupSize = -1.0; //unknown size
	} else {
		auto *lSizeJob = qobject_cast<KIO::DirectorySizeJob *>(pJob);
		mPlan->mLastBackupSize = static_cast<double>(lSizeJob->totalSize());
	}
	mPlan->save();
	exitBackupRunningState(pJob->error() == 0);
}

void FSExecutor::checkMountPoints() {
	QFile lMountsFile(QStringLiteral("/proc/mounts"));
	if(!lMountsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return;
	}
	// don't use atEnd() to detect when finished reading file, size of
	// this special file is 0 but still returns data when read.
	forever {
		QByteArray lLine = lMountsFile.readLine();
		if(lLine.isEmpty()) {
			break;
		}
		QTextStream lTextStream(lLine);
		QString lDevice, lMountPoint;
		lTextStream >> lDevice >> lMountPoint;
		if(lMountPoint == mWatchedParentDir) {
			checkStatus();
		}
	}
}

void MountWatcher::run() {
	int lMountsFd = open("/proc/mounts", O_RDONLY);
	fd_set lFdSet;

	forever {
		FD_ZERO(&lFdSet);
		FD_SET(lMountsFd, &lFdSet);
		if(select(lMountsFd+1, nullptr, nullptr, &lFdSet, nullptr) > 0) {
			emit mountsChanged();
		}
	}
}
