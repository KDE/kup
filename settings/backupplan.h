/*
 * Copyright 2019 Simon Persson <simon.persson@mykolab.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy 
 * defined in Section 14 of version 3 of the license.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef BACKUPPLAN_H
#define BACKUPPLAN_H

#include <KCoreConfigSkeleton>

class Schedule;

class BackupPlan : public KCoreConfigSkeleton
{
public:
	BackupPlan(int pPlanNumber, KSharedConfigPtr pConfig, QObject *pParent = 0);
	int planNumber() const {return mPlanNumber;}
	virtual void setPlanNumber(int pPlanNumber);
	QString statusText();
	void removePlanFromConfig();
	void copyFrom(const BackupPlan &pPlan);

	QString mDescription;
	QStringList mPathsIncluded;
	QStringList mPathsExcluded;
	enum BackupType {BupType = 0, RsyncType};
	qint32 mBackupType;

	enum ScheduleType {MANUAL=0, INTERVAL, USAGE};
	qint32 mScheduleType;
	qint32 mScheduleInterval;
	qint32 mScheduleIntervalUnit;
	qint32 mUsageLimit; // in hours
	bool mAskBeforeTakingBackup;

	qint32 mDestinationType;
	QUrl mFilesystemDestinationPath;
	QString mExternalUUID;
	QString mExternalDestinationPath;
	QString mExternalVolumeLabel;
	QString mExternalDeviceDescription;
	int mExternalPartitionNumber;
	int mExternalPartitionsOnDrive;
	qulonglong mExternalVolumeCapacity;

	bool mShowHiddenFolders;
	bool mGenerateRecoveryInfo;
	bool mCheckBackups;

	QDateTime mLastCompleteBackup;
	// Size of the last backup in bytes.
	double mLastBackupSize;
	// Last known available space on destination
	double mLastAvailableSpace;
	// How long has Kup been running since last backup (s)
	quint32 mAccumulatedUsageTime;

	virtual QDateTime nextScheduledTime();
	virtual qint64 scheduleIntervalInSeconds();

	enum Status {GOOD, MEDIUM, BAD, NO_STATUS};
	Status backupStatus();
	static QString iconName(Status pStatus);

protected:
	void usrRead() override;
	int mPlanNumber;
};

#endif // BACKUPPLAN_H
