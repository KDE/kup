// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef RESTOREDIALOG_H
#define RESTOREDIALOG_H

#include "versionlistmodel.h"

#include <KIO/Job>
#include <KIO/UDSEntry>

#include <QDialog>
#include <QFileInfo>

namespace Ui
{
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
    explicit RestoreDialog(BupSourceInfo pPathInfo, QWidget *parent = nullptr);
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
    qint64 mDestinationSize{}; // size of files about to be overwritten
    qint64 mSourceSize{}; // size of files about to be read
    KMessageWidget *mMessageWidget;
    QString mSavedWorkingDirectory;
    QString mSourceFileName;
    QHash<QString, qint64> mFileSizes;
    int mDirectoriesCount{};
    KWidgetJobTracker *mJobTracker;
};

#endif // RESTOREDIALOG_H
