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

#ifndef RESTOREDIALOG_H
#define RESTOREDIALOG_H

#include "versionlistmodel.h"

#include <KIO/Job>
#include <QDialog>
#include <QFileInfo>

namespace Ui {
class RestoreDialog;
}

class DirSelector;
class KFileWidget;
class KMessageWidget;
class KWidgetJobTracker;
class QTreeWidget;

class RestoreDialog : public QDialog
{
	Q_OBJECT

public:
	explicit RestoreDialog(const BupSourceInfo &pPathInfo, QWidget *parent = nullptr);
	~RestoreDialog() override;

protected:
	void changeEvent(QEvent *pEvent) override;

protected slots:
	void setOriginalDestination();
	void setCustomDestination();
	void checkDestinationSelection();
	void checkDestinationSelection2();
	void startPrechecks();
	void collectSourceListing(KIO::Job *pJob, const KIO::UDSEntryList &pEntryList);
	void sourceListingCompleted(KJob *pJob);
	void completePrechecks();
	void fileOverwriteConfirmed();
	void startRestoring();
	void restoringCompleted(KJob *pJob);
	void fileMoveCompleted(KJob *pJob);
	void folderMoveCompleted(KJob *pJob);
	void createNewFolder();
	void openDestinationFolder();

private:
	void checkForExistingFiles(const KIO::UDSEntryList &pEntryList);
	void moveFolder();
	Ui::RestoreDialog *mUI;
	KFileWidget *mFileWidget;
	DirSelector *mDirSelector;
	QFileInfo mDestination;
	QFileInfo mFolderToCreate;
	QString mRestorationPath; // not necessarily same as destination
	BupSourceInfo mSourceInfo;
	quint64 mDestinationSize; //size of files about to be overwritten
	quint64 mSourceSize; //size of files about to be read
	KMessageWidget *mMessageWidget;
	QString mSavedWorkingDirectory;
	QString mSourceFileName;
	QHash<QString, quint64> mFileSizes;
	int mDirectoriesCount;
	KWidgetJobTracker *mJobTracker;
};

#endif // RESTOREDIALOG_H
