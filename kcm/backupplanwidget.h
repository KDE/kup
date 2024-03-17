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

class FileScanner : public QObject
{
    Q_OBJECT
public:
    FileScanner();
    bool event(QEvent *pEvent) override;

public slots:
    void includePath(const QString &pPath);
    void excludePath(const QString &pPath);

signals:
    void unreadablesChanged(QPair<QSet<QString>, QSet<QString>>);
    void symlinkProblemsChanged(QHash<QString, QString>);

protected slots:
    void sendPendingUnreadables();
    void sendPendingSymlinks();

protected:
    bool isPathIncluded(const QString &pPath);
    void checkPathForProblems(const QFileInfo &pFileInfo);
    bool isSymlinkProblematic(const QString &pTarget);
    void scanFolder(const QString &pPath);

    QSet<QString> mIncludedFolders;
    QSet<QString> mExcludedFolders;

    QSet<QString> mUnreadableFolders;
    QSet<QString> mUnreadableFiles;
    QTimer *mUnreadablesTimer;

    QHash<QString, QString> mSymlinksNotOk;
    QHash<QString, QString> mSymlinksOk;
    QTimer *mSymlinkTimer;
};

class FolderSelectionWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FolderSelectionWidget(FolderSelectionModel *pModel, QWidget *pParent = nullptr);
    virtual ~FolderSelectionWidget();

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
    FolderSelectionModel *mModel;
    KMessageWidget *mMessageWidget;
    QThread *mWorkerThread;
    QStringList mUnreadableFolders;
    QStringList mUnreadableFiles;
    QString mExcludeActionPath;
    QAction *mExcludeAction;
    QHash<QString, QString> mSymlinkProblems;
    QString mIncludeActionPath;
    QAction *mIncludeAction;
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
    KMessageWidget *mLocalMessage;
    KMessageWidget *mExistMessage;

protected slots:
    void openDriveDestDialog();
    void checkFilesystemDestination(const QString &pDestination);

signals:
    void requestOverviewReturn();
};

#endif // BACKUPPLANWIDGET_H
