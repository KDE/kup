// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef BACKUPPLANWIDGET_H
#define BACKUPPLANWIDGET_H

#include <QDialog>
#include <QSet>
#include <QWidget>

class BackupPlan;
class DirSelector;
class DriveSelection;
class FolderSelectionModel;

class KLineEdit;
class KMessageWidget;
class KPageWidget;
class KPageWidgetItem;
class QAction;
class QFileInfo;
class QPushButton;
class QRadioButton;
class QThread;
class QTimer;
class QTreeView;

class FolderSelectionWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FolderSelectionWidget(FolderSelectionModel *pModel, BackupPlan *pBackupPlan, QWidget *pParent = nullptr);
    virtual ~FolderSelectionWidget();
    FolderSelectionModel *mModel;

public slots:
    void setHiddenFoldersVisible(bool pVisible);
    void expandToShowSelections();
    void setUnreadables(const QPair<QSet<QString>, QSet<QString>> &pUnreadables);
    void setSymlinks(QHash<QString, QString> pSymlinks);
    void updateMessage();
    void executeExcludeAction();
    void executeIncludeAction();

protected:
    QTreeView *mTreeView;
    KMessageWidget *mMessageWidget;
    QThread *mWorkerThread;
    QStringList mUnreadableFolders;
    QStringList mUnreadableFiles;
    QString mExcludeActionPath;
    QAction *mExcludeAction;
    QHash<QString, QString> mSymlinkProblems;
    QString mIncludeActionPath;
    QAction *mIncludeAction;
    BackupPlan *mBackupPlan;
};

class ConfigIncludeDummy : public QWidget
{
    Q_OBJECT
signals:
    void includeListChanged();

public:
    Q_PROPERTY(QStringList includeList READ includeList WRITE setIncludeList NOTIFY includeListChanged USER true)
    ConfigIncludeDummy(FolderSelectionModel *pModel, FolderSelectionWidget *pParent);
    QStringList includeList();
    void setIncludeList(QStringList pIncludeList);
    FolderSelectionModel *mModel;
    FolderSelectionWidget *mTreeView;
};

class ConfigExcludeDummy : public QWidget
{
    Q_OBJECT
signals:
    void excludeListChanged();

public:
    Q_PROPERTY(QStringList excludeList READ excludeList WRITE setExcludeList NOTIFY excludeListChanged USER true)
    ConfigExcludeDummy(FolderSelectionModel *pModel, FolderSelectionWidget *pParent);
    QStringList excludeList();
    void setExcludeList(QStringList pExcludeList);
    FolderSelectionModel *mModel;
    FolderSelectionWidget *mTreeView;
};

class DirDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DirDialog(const QUrl &pRootDir, const QString &pStartSubDir, QWidget *pParent = nullptr);
    QUrl url() const;

private:
    DirSelector *mDirSelector;
};

class BackupPlanWidget : public QWidget
{
    Q_OBJECT
public:
    BackupPlanWidget(BackupPlan *pBackupPlan, const QString &pBupVersion, const QString &pRsyncVersion, bool pPar2Available);

    void saveExtraData();
    void showSourcePage();
    KLineEdit *mDescriptionEdit;

protected:
    KPageWidgetItem *createTypePage(const QString &pBupVersion, const QString &pRsyncVersion);
    KPageWidgetItem *createSourcePage();
    KPageWidgetItem *createDestinationPage();
    KPageWidgetItem *createSchedulePage();
    KPageWidgetItem *createAdvancedPage(bool pPar2Available);

    QPushButton *mConfigureButton;
    KPageWidget *mConfigPages;
    BackupPlan *mBackupPlan;
    DriveSelection *mDriveSelection{};
    KLineEdit *mDriveDestEdit{};
    QRadioButton *mVersionedRadio{};
    QRadioButton *mSyncedRadio{};
    FolderSelectionWidget *mSourceSelectionWidget{};
    KPageWidgetItem *mSourcePage;
    KPageWidgetItem *mAdvancedPage;
    KMessageWidget *mLocalMessage;
    KMessageWidget *mExistMessage;

protected slots:
    void openDriveDestDialog();
    void checkFilesystemDestination(const QString &pDestination);

signals:
    void requestOverviewReturn();
};

#endif // BACKUPPLANWIDGET_H
