// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "backupplanwidget.h"
#include "backupplan.h"
#include "dirselector.h"
#include "driveselection.h"
#include "folderselectionmodel.h"
#include "kbuttongroup.h"

#include <QAction>
#include <QBoxLayout>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QEvent>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QThread>
#include <QTimer>
#include <QTreeView>

#include <KComboBox>
#include <KConfigDialogManager>
#include <KConfigGroup>
#include <KIO/OpenUrlJob>
#include <KLineEdit>
#include <KLocalizedString>
#include <KMessageWidget>
#include <KPageWidget>
#include <KUrlCompletion>
#include <KUrlRequester>
#include <utility>

class ScanFolderEvent : public QEvent
{
public:
    explicit ScanFolderEvent(QString pPath)
        : QEvent(eventType)
        , mPath(std::move(pPath))
    {
    }
    QString mPath;
    static const QEvent::Type eventType = static_cast<QEvent::Type>(QEvent::User + 1);
};

FileScanner::FileScanner()
{
    // create a timer that will call a slot to send the pending updates to UI, one second
    // after the last update comes in, just to minimize risk of showing incomplete
    // information to the user.
    mUnreadablesTimer = new QTimer(this);
    mUnreadablesTimer->setSingleShot(true);
    mUnreadablesTimer->setInterval(1000);
    connect(mUnreadablesTimer, &QTimer::timeout, this, &FileScanner::sendPendingUnreadables);

    mSymlinkTimer = new QTimer(this);
    mSymlinkTimer->setSingleShot(true);
    mSymlinkTimer->setInterval(1000);
    connect(mSymlinkTimer, &QTimer::timeout, this, &FileScanner::sendPendingSymlinks);
}

bool FileScanner::event(QEvent *pEvent)
{
    if (pEvent->type() == ScanFolderEvent::eventType) {
        auto lEvent = dynamic_cast<ScanFolderEvent *>(pEvent);
        if (isPathIncluded(lEvent->mPath)) {
            scanFolder(lEvent->mPath);
        }
        return true;
    }
    return QObject::event(pEvent);
}

void FileScanner::includePath(const QString &pPath)
{
    if (!mExcludedFolders.remove(pPath)) {
        mIncludedFolders += pPath;
    }
    checkPathForProblems(QFileInfo(pPath));

    QMutableHashIterator<QString, QString> i(mSymlinksNotOk);
    while (i.hasNext()) {
        i.next();
        if (isPathIncluded(i.value())) {
            mSymlinksOk.insert(i.key(), i.value());
            i.remove();
            mSymlinkTimer->start();
        }
    }
}

void FileScanner::excludePath(const QString &pPath)
{
    if (!mIncludedFolders.remove(pPath)) {
        mExcludedFolders += pPath;
    }
    QString lPath = pPath + QStringLiteral("/");
    QSet<QString>::iterator it = mUnreadableFiles.begin();
    while (it != mUnreadableFiles.end()) {
        if (it->startsWith(lPath)) {
            mUnreadablesTimer->start();
            it = mUnreadableFiles.erase(it);
        } else {
            ++it;
        }
    }
    it = mUnreadableFolders.begin();
    while (it != mUnreadableFolders.end()) {
        if (it->startsWith(lPath) || *it == pPath) {
            mUnreadablesTimer->start();
            it = mUnreadableFolders.erase(it);
        } else {
            ++it;
        }
    }

    QMutableHashIterator<QString, QString> i(mSymlinksNotOk);
    while (i.hasNext()) {
        if (!isPathIncluded(i.next().key())) {
            i.remove();
            mSymlinkTimer->start();
        }
    }

    i = mSymlinksOk;
    while (i.hasNext()) {
        i.next();
        if (!isPathIncluded(i.key())) {
            i.remove();
        } else if (isSymlinkProblematic(i.value())) {
            mSymlinksNotOk.insert(i.key(), i.value());
            mSymlinkTimer->start();
            i.remove();
        }
    }
}

void FileScanner::sendPendingUnreadables()
{
    emit unreadablesChanged(QPair<QSet<QString>, QSet<QString>>(mUnreadableFolders, mUnreadableFiles));
}

void FileScanner::sendPendingSymlinks()
{
    emit symlinkProblemsChanged(mSymlinksNotOk);
}

bool FileScanner::isPathIncluded(const QString &pPath)
{
    int lLongestInclude = 0;
    foreach (const QString &lPath, mIncludedFolders) {
        bool lMatches = pPath == lPath || pPath.startsWith(lPath + QStringLiteral("/"));
        if (lMatches && lPath.length() > lLongestInclude) {
            lLongestInclude = lPath.length();
        }
    }
    int lLongestExclude = 0;
    foreach (const QString &lPath, mExcludedFolders) {
        bool lMatches = pPath == lPath || pPath.startsWith(lPath + QStringLiteral("/"));
        if (lMatches && lPath.length() > lLongestExclude) {
            lLongestExclude = lPath.length();
        }
    }
    return lLongestInclude > lLongestExclude;
}

void FileScanner::checkPathForProblems(const QFileInfo &pFileInfo)
{
    if (pFileInfo.isSymLink()) {
        if (isSymlinkProblematic(pFileInfo.symLinkTarget())) {
            mSymlinksNotOk.insert(pFileInfo.absoluteFilePath(), pFileInfo.symLinkTarget());
            mSymlinkTimer->start();
        } else {
            mSymlinksOk.insert(pFileInfo.absoluteFilePath(), pFileInfo.symLinkTarget());
        }
    } else if (pFileInfo.isDir()) {
        QCoreApplication::postEvent(this, new ScanFolderEvent(pFileInfo.absoluteFilePath()), Qt::LowEventPriority);
    } else {
        if (!pFileInfo.isReadable()) {
            mUnreadableFiles += pFileInfo.absoluteFilePath();
            mUnreadablesTimer->start();
        }
    }
}

bool FileScanner::isSymlinkProblematic(const QString &pTarget)
{
    QFileInfo lTargetInfo(pTarget);
    return lTargetInfo.exists() && !isPathIncluded(pTarget) && pTarget.startsWith(QStringLiteral("/home/"));
}

void FileScanner::scanFolder(const QString &pPath)
{
    QDir lDir(pPath);
    if (!lDir.isReadable()) {
        mUnreadableFolders += pPath;
        mUnreadablesTimer->start();
    } else {
        QFileInfoList lInfoList = lDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
        foreach (const QFileInfo &lFileInfo, lInfoList) {
            checkPathForProblems(lFileInfo);
        }
    }
}

ConfigIncludeDummy::ConfigIncludeDummy(FolderSelectionModel *pModel, FolderSelectionWidget *pParent)
    : QWidget(pParent)
    , mModel(pModel)
    , mTreeView(pParent)
{
    connect(mModel, &FolderSelectionModel::includedPathAdded, this, &ConfigIncludeDummy::includeListChanged);
    connect(mModel, &FolderSelectionModel::includedPathRemoved, this, &ConfigIncludeDummy::includeListChanged);
}

QStringList ConfigIncludeDummy::includeList()
{
    QStringList lList = mModel->includedPaths().values();
    lList.sort();
    return lList;
}

void ConfigIncludeDummy::setIncludeList(QStringList pIncludeList)
{
    for (int i = 0; i < pIncludeList.count(); ++i) {
        if (!QFile::exists(pIncludeList.at(i))) {
            pIncludeList.removeAt(i--);
        }
    }
    mModel->setIncludedPaths(QSet<QString>(pIncludeList.begin(), pIncludeList.end()));
    mTreeView->expandToShowSelections();
}

ConfigExcludeDummy::ConfigExcludeDummy(FolderSelectionModel *pModel, FolderSelectionWidget *pParent)
    : QWidget(pParent)
    , mModel(pModel)
    , mTreeView(pParent)
{
    connect(mModel, &FolderSelectionModel::excludedPathAdded, this, &ConfigExcludeDummy::excludeListChanged);
    connect(mModel, &FolderSelectionModel::excludedPathRemoved, this, &ConfigExcludeDummy::excludeListChanged);
}

QStringList ConfigExcludeDummy::excludeList()
{
    QStringList lList = mModel->excludedPaths().values();
    lList.sort();
    return lList;
}

void ConfigExcludeDummy::setExcludeList(QStringList pExcludeList)
{
    for (int i = 0; i < pExcludeList.count(); ++i) {
        if (!QFile::exists(pExcludeList.at(i))) {
            pExcludeList.removeAt(i--);
        }
    }
    mModel->setExcludedPaths(QSet<QString>(pExcludeList.begin(), pExcludeList.end()));
    mTreeView->expandToShowSelections();
}

FolderSelectionWidget::FolderSelectionWidget(FolderSelectionModel *pModel, QWidget *pParent)
    : QWidget(pParent)
    , mModel(pModel)
{
    mMessageWidget = new KMessageWidget(this);
    mMessageWidget->setCloseButtonVisible(false);
    mMessageWidget->setWordWrap(true);
    mMessageWidget->hide();
    mTreeView = new QTreeView(this);
    auto lVLayout = new QVBoxLayout;
    lVLayout->addWidget(mMessageWidget);
    lVLayout->addWidget(mTreeView, 1);
    setLayout(lVLayout);

    connect(mMessageWidget, &KMessageWidget::hideAnimationFinished, this, &FolderSelectionWidget::updateMessage);

    mExcludeAction = new QAction(xi18nc("@action:button", "Exclude Folder"), this);
    connect(mExcludeAction, &QAction::triggered, this, &FolderSelectionWidget::executeExcludeAction);

    mIncludeAction = new QAction(xi18nc("@action:button", "Include Folder"), this);
    connect(mIncludeAction, &QAction::triggered, this, &FolderSelectionWidget::executeIncludeAction);

    mModel->setRootPath(QStringLiteral("/"));
    mModel->setParent(this);
    mTreeView->setAnimated(true);
    mTreeView->setModel(mModel);
    mTreeView->setHeaderHidden(true);
    // always expand the home folder, prevents problem with empty include&exclude lists.
    QModelIndex lIndex = mModel->index(QDir::homePath());
    while (lIndex.isValid()) {
        mTreeView->expand(lIndex);
        lIndex = lIndex.parent();
    }

    auto lIncludeDummy = new ConfigIncludeDummy(mModel, this);
    lIncludeDummy->setObjectName(QStringLiteral("kcfg_Paths included"));
    auto lExcludeDummy = new ConfigExcludeDummy(mModel, this);
    lExcludeDummy->setObjectName(QStringLiteral("kcfg_Paths excluded"));

    qRegisterMetaType<QPair<QSet<QString>, QSet<QString>>>("QPair<QSet<QString>,QSet<QString>>");
    qRegisterMetaType<QHash<QString, QString>>("QHash<QString,QString>");

    mWorkerThread = new QThread(this);
    auto lFileScanner = new FileScanner;
    lFileScanner->moveToThread(mWorkerThread);
    connect(mWorkerThread, &QThread::finished, lFileScanner, &QObject::deleteLater);

    connect(mModel, &FolderSelectionModel::includedPathAdded, lFileScanner, &FileScanner::includePath);
    connect(mModel, &FolderSelectionModel::excludedPathRemoved, lFileScanner, &FileScanner::includePath);
    connect(mModel, &FolderSelectionModel::excludedPathAdded, lFileScanner, &FileScanner::excludePath);
    connect(mModel, &FolderSelectionModel::includedPathRemoved, lFileScanner, &FileScanner::excludePath);
    connect(lFileScanner, &FileScanner::unreadablesChanged, this, &FolderSelectionWidget::setUnreadables);
    connect(lFileScanner, &FileScanner::symlinkProblemsChanged, this, &FolderSelectionWidget::setSymlinks);
    mWorkerThread->start();
}

FolderSelectionWidget::~FolderSelectionWidget()
{
    mWorkerThread->quit();
    mWorkerThread->wait();
}

void FolderSelectionWidget::setHiddenFoldersVisible(bool pVisible)
{
    mModel->setHiddenFoldersVisible(pVisible);
    // give the filesystem model some time to refresh after changing filtering
    // before expanding folders again.
    if (pVisible) {
        QTimer::singleShot(2000, this, SLOT(expandToShowSelections()));
    }
}

void FolderSelectionWidget::expandToShowSelections()
{
    foreach (const QString &lFolder, mModel->includedPaths() + mModel->excludedPaths()) {
        QFileInfo lFolderInfo(lFolder);
        bool lShouldBeShown = true;
        while (lFolderInfo.absoluteFilePath() != QStringLiteral("/")) {
            if (lFolderInfo.isHidden() && !mModel->hiddenFoldersVisible()) {
                lShouldBeShown = false; // skip if this folder should not be shown.
                break;
            }
            lFolderInfo = QFileInfo(lFolderInfo.absolutePath()); // move up one level
        }
        if (lShouldBeShown) {
            QModelIndex lIndex = mModel->index(lFolder).parent();
            while (lIndex.isValid()) {
                mTreeView->expand(lIndex);
                lIndex = lIndex.parent();
            }
        }
    }
}

void FolderSelectionWidget::setUnreadables(const QPair<QSet<QString>, QSet<QString>> &pUnreadables)
{
    mUnreadableFolders = pUnreadables.first.values();
    mUnreadableFiles = pUnreadables.second.values();
    updateMessage();
}

void FolderSelectionWidget::setSymlinks(QHash<QString, QString> pSymlinks)
{
    mSymlinkProblems = std::move(pSymlinks);
    updateMessage();
}

void FolderSelectionWidget::updateMessage()
{
    if (mMessageWidget->isVisible() || mMessageWidget->isHideAnimationRunning()) {
        mMessageWidget->animatedHide();
        return;
    }

    mMessageWidget->removeAction(mExcludeAction);
    mMessageWidget->removeAction(mIncludeAction);

    if (!mUnreadableFolders.isEmpty()) {
        mMessageWidget->setMessageType(KMessageWidget::Error);
        mMessageWidget->setText(xi18nc("@info message bar appearing on top",
                                       "You don't have permission to read this folder: <filename>%1</filename><nl/>"
                                       "It cannot be included in the source selection. "
                                       "If it does not contain anything important to you, one possible "
                                       "solution is to exclude the folder from the backup plan.",
                                       mUnreadableFolders.first()));
        mExcludeActionPath = mUnreadableFolders.first();
        mMessageWidget->addAction(mExcludeAction);
        mMessageWidget->animatedShow();
    } else if (!mUnreadableFiles.isEmpty()) {
        mMessageWidget->setMessageType(KMessageWidget::Error);
        mMessageWidget->setText(xi18nc("@info message bar appearing on top",
                                       "You don't have permission to read this file: <filename>%1</filename><nl/>"
                                       "It cannot be included in the source selection. "
                                       "If the file is not important to you, one possible solution is "
                                       "to exclude the whole folder where the file is stored from the backup plan.",
                                       mUnreadableFiles.first()));
        QFileInfo lFileInfo(mUnreadableFiles.first());
        mExcludeActionPath = lFileInfo.absolutePath();
        mMessageWidget->addAction(mExcludeAction);
        mMessageWidget->animatedShow();
    } else if (!mSymlinkProblems.isEmpty()) {
        mMessageWidget->setMessageType(KMessageWidget::Warning);
        QHashIterator<QString, QString> i(mSymlinkProblems);
        i.next();
        QFileInfo lFileInfo(i.value());
        if (lFileInfo.isDir()) {
            mMessageWidget->setText(xi18nc("@info message bar appearing on top",
                                           "The symbolic link <filename>%1</filename> points to a folder which "
                                           "is not included: <filename>%2</filename>.<nl/>That is probably not "
                                           "what you want. One solution is to simply include the target folder in the "
                                           "backup plan.",
                                           i.key(),
                                           i.value()));
            mIncludeActionPath = i.value();
        } else {
            mMessageWidget->setText(xi18nc("@info message bar appearing on top",
                                           "The symbolic link <filename>%1</filename> points to a file which "
                                           "is not included: <filename>%2</filename>.<nl/>That is probably not "
                                           "what you want. One solution is to simply include the folder where the file "
                                           "is stored in the backup plan.",
                                           i.key(),
                                           i.value()));
            mIncludeActionPath = lFileInfo.absolutePath();
        }
        mMessageWidget->addAction(mIncludeAction);
        mMessageWidget->animatedShow();
    }
}

void FolderSelectionWidget::executeExcludeAction()
{
    mModel->excludePath(mExcludeActionPath);
}

void FolderSelectionWidget::executeIncludeAction()
{
    mModel->includePath(mIncludeActionPath);
}

DirDialog::DirDialog(const QUrl &pRootDir, const QString &pStartSubDir, QWidget *pParent)
    : QDialog(pParent)
{
    setWindowTitle(xi18nc("@title:window", "Select Folder"));

    mDirSelector = new DirSelector(this);
    mDirSelector->setRootUrl(pRootDir);
    QUrl lSubUrl = QUrl::fromLocalFile(pRootDir.adjusted(QUrl::StripTrailingSlash).path() + '/' + pStartSubDir);
    mDirSelector->expandToUrl(lSubUrl);

    auto lButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(lButtonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(lButtonBox, SIGNAL(rejected()), this, SLOT(reject()));
    auto lNewFolderButton = new QPushButton(xi18nc("@action:button", "New Folder..."));
    connect(lNewFolderButton, SIGNAL(clicked()), mDirSelector, SLOT(createNewFolder()));
    lButtonBox->addButton(lNewFolderButton, QDialogButtonBox::ActionRole);
    QPushButton *lOkButton = lButtonBox->button(QDialogButtonBox::Ok);
    lOkButton->setDefault(true);
    lOkButton->setShortcut(Qt::Key_Return);

    auto lMainLayout = new QVBoxLayout;
    lMainLayout->addWidget(mDirSelector);
    lMainLayout->addWidget(lButtonBox);
    setLayout(lMainLayout);

    mDirSelector->setFocus();
}

QUrl DirDialog::url() const
{
    return mDirSelector->url();
}

BackupPlanWidget::BackupPlanWidget(BackupPlan *pBackupPlan, const QString &pBupVersion, const QString &pRsyncVersion, bool pPar2Available)
    : mBackupPlan(pBackupPlan)
{
    mDescriptionEdit = new KLineEdit;
    mDescriptionEdit->setObjectName(QStringLiteral("kcfg_Description"));
    mDescriptionEdit->setClearButtonEnabled(true);
    auto lDescriptionLabel = new QLabel(xi18nc("@label", "Description:"));
    lDescriptionLabel->setBuddy(mDescriptionEdit);
    mConfigureButton = new QPushButton(QIcon::fromTheme(QStringLiteral("go-previous-view")), xi18nc("@action:button", "Back to overview"));
    connect(mConfigureButton, SIGNAL(clicked()), this, SIGNAL(requestOverviewReturn()));

    mConfigPages = new KPageWidget;
    mConfigPages->addPage(createTypePage(pBupVersion, pRsyncVersion));
    mSourcePage = createSourcePage();
    mConfigPages->addPage(mSourcePage);
    mConfigPages->addPage(createDestinationPage());
    mConfigPages->addPage(createSchedulePage());
    mConfigPages->addPage(createAdvancedPage(pPar2Available));

    auto lHLayout1 = new QHBoxLayout;
    lHLayout1->addWidget(mConfigureButton);
    lHLayout1->addStretch();
    lHLayout1->addWidget(lDescriptionLabel);
    lHLayout1->addWidget(mDescriptionEdit);

    auto lVLayout1 = new QVBoxLayout;
    lVLayout1->addLayout(lHLayout1);
    lVLayout1->addWidget(mConfigPages);
    lVLayout1->setSpacing(0);
    setLayout(lVLayout1);
}

void BackupPlanWidget::saveExtraData()
{
    mDriveSelection->saveExtraData();
}

void BackupPlanWidget::showSourcePage()
{
    mConfigPages->setCurrentPage(mSourcePage);
}

KPageWidgetItem *BackupPlanWidget::createTypePage(const QString &pBupVersion, const QString &pRsyncVersion)
{
    mVersionedRadio = new QRadioButton;
    QString lVersionedInfo = xi18nc("@info",
                                    "This type of backup is an <emphasis>archive</emphasis>. It contains both "
                                    "the latest version of your files and earlier backed up versions. "
                                    "Using this type of backup allows you to recover older versions of your "
                                    "files, or files which were deleted on your computer at a later time. "
                                    "The storage space needed is minimized by looking for common parts of "
                                    "your files between versions and only storing those parts once. "
                                    "Nevertheless, the backup archive will keep growing in size as time goes by.<nl/>"
                                    "Also important to know is that the files in the archive can not be accessed "
                                    "directly with a general file manager, a special program is needed.");
    auto lVersionedInfoLabel = new QLabel(lVersionedInfo);
    lVersionedInfoLabel->setWordWrap(true);
    auto lVersionedWidget = new QWidget;
    lVersionedWidget->setVisible(false);
    QObject::connect(mVersionedRadio, SIGNAL(toggled(bool)), lVersionedWidget, SLOT(setVisible(bool)));
    if (pBupVersion.isEmpty()) {
        mVersionedRadio->setText(xi18nc("@option:radio",
                                        "Versioned Backup (not available "
                                        "because <application>bup</application> is not installed)"));
        mVersionedRadio->setEnabled(false);
        lVersionedWidget->setEnabled(false);
    } else {
        mVersionedRadio->setText(xi18nc("@option:radio", "Versioned Backup (recommended)"));
    }

    mSyncedRadio = new QRadioButton;
    QString lSyncedInfo = xi18nc("@info",
                                 "This type of backup is a folder which is synchronized with your "
                                 "selected source folders. Saving a backup simply means making the "
                                 "backup destination contain an exact copy of your source folders as "
                                 "they are now and nothing else. If a file has been deleted in a "
                                 "source folder it will get deleted from the backup folder.<nl/>"
                                 "This type of backup can protect you against data loss due to a broken "
                                 "hard drive but it does not help you to recover from your own mistakes.");
    auto lSyncedInfoLabel = new QLabel(lSyncedInfo);
    lSyncedInfoLabel->setWordWrap(true);
    auto lSyncedWidget = new QWidget;
    lSyncedWidget->setVisible(false);
    QObject::connect(mSyncedRadio, SIGNAL(toggled(bool)), lSyncedWidget, SLOT(setVisible(bool)));
    if (pRsyncVersion.isEmpty()) {
        mSyncedRadio->setText(xi18nc("@option:radio",
                                     "Synchronized Backup (not available "
                                     "because <application>rsync</application> is not installed)"));
        mSyncedRadio->setEnabled(false);
        lSyncedWidget->setEnabled(false);
    } else {
        mSyncedRadio->setText(xi18nc("@option:radio", "Synchronized Backup"));
    }
    auto lButtonGroup = new KButtonGroup;
    lButtonGroup->setObjectName(QStringLiteral("kcfg_Backup type"));
    lButtonGroup->setFlat(true);
    int lIndentation =
        lButtonGroup->style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth) + lButtonGroup->style()->pixelMetric(QStyle::PM_RadioButtonLabelSpacing);

    auto lVersionedVLayout = new QGridLayout;
    lVersionedVLayout->setColumnMinimumWidth(0, lIndentation);
    lVersionedVLayout->setContentsMargins(0, 0, 0, 0);
    lVersionedVLayout->addWidget(lVersionedInfoLabel, 0, 1);
    lVersionedWidget->setLayout(lVersionedVLayout);

    auto lSyncedVLayout = new QGridLayout;
    lSyncedVLayout->setColumnMinimumWidth(0, lIndentation);
    lSyncedVLayout->setContentsMargins(0, 0, 0, 0);
    lSyncedVLayout->addWidget(lSyncedInfoLabel, 0, 1);
    lSyncedWidget->setLayout(lSyncedVLayout);

    auto lVLayout = new QVBoxLayout;
    lVLayout->addWidget(mVersionedRadio);
    lVLayout->addWidget(lVersionedWidget);
    lVLayout->addWidget(mSyncedRadio);
    lVLayout->addWidget(lSyncedWidget);
    lVLayout->addStretch();
    lButtonGroup->setLayout(lVLayout);
    auto lPage = new KPageWidgetItem(lButtonGroup);
    lPage->setName(xi18nc("@title", "Backup Type"));
    lPage->setHeader(xi18nc("@label", "Select what type of backup you want"));
    lPage->setIcon(QIcon::fromTheme(QStringLiteral("folder-sync")));
    return lPage;
}

KPageWidgetItem *BackupPlanWidget::createSourcePage()
{
    mSourceSelectionWidget = new FolderSelectionWidget(new FolderSelectionModel(mBackupPlan->mShowHiddenFolders), this);
    auto lPage = new KPageWidgetItem(mSourceSelectionWidget);
    lPage->setName(xi18nc("@title", "Sources"));
    lPage->setHeader(xi18nc("@label", "Select which folders to include in backup"));
    lPage->setIcon(QIcon::fromTheme(QStringLiteral("cloud-upload")));
    return lPage;
}

KPageWidgetItem *BackupPlanWidget::createDestinationPage()
{
    auto lButtonGroup = new KButtonGroup(this);
    lButtonGroup->setObjectName(QStringLiteral("kcfg_Destination type"));
    lButtonGroup->setFlat(true);

    int lIndentation =
        lButtonGroup->style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth) + lButtonGroup->style()->pixelMetric(QStyle::PM_RadioButtonLabelSpacing);

    auto lVLayout = new QVBoxLayout;
    auto lFileSystemRadio = new QRadioButton(xi18nc("@option:radio", "Filesystem Path"));
    auto lDriveRadio = new QRadioButton(xi18nc("@option:radio", "External Storage"));

    auto lFileSystemWidget = new QWidget;
    lFileSystemWidget->setVisible(false);
    QObject::connect(lFileSystemRadio, SIGNAL(toggled(bool)), lFileSystemWidget, SLOT(setVisible(bool)));
    auto lFileSystemInfoLabel = new QLabel(xi18nc("@info",
                                                  "You can use this option for backing up to a secondary "
                                                  "internal harddrive, an external eSATA drive or networked "
                                                  "storage. The requirement is just that you always mount "
                                                  "it at the same path in the filesystem. The path "
                                                  "specified here does not need to exist at all times, its "
                                                  "existence will be monitored."));
    lFileSystemInfoLabel->setWordWrap(true);
    auto lFileSystemLabel = new QLabel(xi18nc("@label:textbox", "Destination Path for Backup:"));
    auto lFileSystemUrlEdit = new KUrlRequester;
    lFileSystemUrlEdit->setMode(KFile::Directory | KFile::LocalOnly);
    lFileSystemUrlEdit->setObjectName(QStringLiteral("kcfg_Filesystem destination path"));
    lFileSystemUrlEdit->setStartDir(QUrl::fromLocalFile(QDir::homePath()));
    QObject::connect(lFileSystemUrlEdit, &KUrlRequester::textChanged, this, &BackupPlanWidget::checkFilesystemDestination);

    mLocalMessage = new KMessageWidget(this);
    mLocalMessage->setCloseButtonVisible(false);
    mLocalMessage->setWordWrap(true);
    mLocalMessage->setMessageType(KMessageWidget::Warning);
    mLocalMessage->setText(xi18nc("@info message bar near text edit", "Only local filesystem paths will work!"));
    mLocalMessage->hide();

    mExistMessage = new KMessageWidget(this);
    mExistMessage->setCloseButtonVisible(false);
    mExistMessage->setWordWrap(true);
    mExistMessage->setMessageType(KMessageWidget::Warning);
    mExistMessage->setText(xi18nc("@info message bar near text edit", "Folder does not exist! No backups will be saved until you create it."));
    mExistMessage->hide();

    auto lFileSystemVLayout = new QGridLayout;
    lFileSystemVLayout->setColumnMinimumWidth(0, lIndentation);
    lFileSystemVLayout->setContentsMargins(0, 0, 0, 0);
    lFileSystemVLayout->addWidget(lFileSystemInfoLabel, 0, 1);
    auto lFileSystemHLayout = new QHBoxLayout;
    lFileSystemHLayout->addWidget(lFileSystemLabel);
    lFileSystemHLayout->addWidget(lFileSystemUrlEdit, 1);
    lFileSystemVLayout->addLayout(lFileSystemHLayout, 1, 1);
    lFileSystemVLayout->addWidget(mLocalMessage, 2, 1);
    lFileSystemVLayout->addWidget(mExistMessage, 3, 1);
    lFileSystemWidget->setLayout(lFileSystemVLayout);

    auto lDriveWidget = new QWidget;
    lDriveWidget->setVisible(false);
    QObject::connect(lDriveRadio, SIGNAL(toggled(bool)), lDriveWidget, SLOT(setVisible(bool)));
    auto lDriveInfoLabel = new QLabel(xi18nc("@info",
                                             "Use this option if you want to backup your "
                                             "files on an external storage that can be plugged in "
                                             "to this computer, such as a USB hard drive or memory "
                                             "stick."));
    lDriveInfoLabel->setWordWrap(true);
    mDriveSelection = new DriveSelection(mBackupPlan);
    mDriveSelection->setObjectName(QStringLiteral("kcfg_External drive UUID"));
    mDriveDestEdit = new KLineEdit;
    mDriveDestEdit->setObjectName(QStringLiteral("kcfg_External drive destination path"));
    mDriveDestEdit->setToolTip(xi18nc("@info:tooltip", "The specified folder will be created if it does not exist."));
    mDriveDestEdit->setClearButtonEnabled(true);
    auto lDriveDestLabel = new QLabel(xi18nc("@label:textbox", "Folder on Destination Drive:"));
    lDriveDestLabel->setToolTip(xi18nc("@info:tooltip", "The specified folder will be created if it does not exist."));
    lDriveDestLabel->setBuddy(mDriveDestEdit);
    auto lDriveDestButton = new QPushButton;
    lDriveDestButton->setIcon(QIcon::fromTheme(QStringLiteral("document-open")));
    int lButtonSize = lDriveDestButton->sizeHint().expandedTo(mDriveDestEdit->sizeHint()).height();
    lDriveDestButton->setFixedSize(lButtonSize, lButtonSize);
    lDriveDestButton->setToolTip(xi18nc("@info:tooltip", "Open dialog to select a folder"));
    lDriveDestButton->setEnabled(false);
    connect(mDriveSelection, SIGNAL(selectedDriveIsAccessibleChanged(bool)), lDriveDestButton, SLOT(setEnabled(bool)));
    connect(lDriveDestButton, SIGNAL(clicked()), SLOT(openDriveDestDialog()));
    auto lDriveDestWidget = new QWidget;
    lDriveDestWidget->setVisible(false);
    connect(mDriveSelection, SIGNAL(driveIsSelectedChanged(bool)), lDriveDestWidget, SLOT(setVisible(bool)));
    connect(mSyncedRadio, SIGNAL(toggled(bool)), mDriveSelection, SLOT(updateSyncWarning(bool)));

    auto lDriveVLayout = new QGridLayout;
    lDriveVLayout->setColumnMinimumWidth(0, lIndentation);
    lDriveVLayout->setContentsMargins(0, 0, 0, 0);
    lDriveVLayout->addWidget(lDriveInfoLabel, 0, 1);
    lDriveVLayout->addWidget(mDriveSelection, 1, 1);
    auto lDriveHLayout = new QHBoxLayout;
    lDriveHLayout->addWidget(lDriveDestLabel);
    lDriveHLayout->addWidget(mDriveDestEdit, 1);
    lDriveHLayout->addWidget(lDriveDestButton);
    lDriveDestWidget->setLayout(lDriveHLayout);
    lDriveVLayout->addWidget(lDriveDestWidget, 2, 1);
    lDriveWidget->setLayout(lDriveVLayout);

    lVLayout->addWidget(lFileSystemRadio);
    lVLayout->addWidget(lFileSystemWidget);
    lVLayout->addWidget(lDriveRadio);
    lVLayout->addWidget(lDriveWidget, 1);
    lVLayout->addStretch();
    lButtonGroup->setLayout(lVLayout);

    auto lPage = new KPageWidgetItem(lButtonGroup);
    lPage->setName(xi18nc("@title", "Destination"));
    lPage->setHeader(xi18nc("@label", "Select the backup destination"));
    lPage->setIcon(QIcon::fromTheme(QStringLiteral("cloud-download")));
    return lPage;
}

KPageWidgetItem *BackupPlanWidget::createSchedulePage()
{
    auto lTopWidget = new QWidget(this);
    auto lTopLayout = new QVBoxLayout;
    auto lButtonGroup = new KButtonGroup;
    lButtonGroup->setObjectName(QStringLiteral("kcfg_Schedule type"));
    lButtonGroup->setFlat(true);

    int lIndentation =
        lButtonGroup->style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth) + lButtonGroup->style()->pixelMetric(QStyle::PM_RadioButtonLabelSpacing);

    auto lVLayout = new QVBoxLayout;
    lVLayout->setContentsMargins(0, 0, 0, 0);
    auto lManualRadio = new QRadioButton(xi18nc("@option:radio", "Manual Activation"));
    auto lIntervalRadio = new QRadioButton(xi18nc("@option:radio", "Interval"));
    auto lUsageRadio = new QRadioButton(xi18nc("@option:radio", "Active Usage Time"));

    auto lManualLabel = new QLabel(xi18nc("@info",
                                          "Backups are only saved when manually requested. "
                                          "This can be done by using the Backup Status "
                                          "plasma widget."));
    lManualLabel->setVisible(false);
    lManualLabel->setWordWrap(true);
    connect(lManualRadio, SIGNAL(toggled(bool)), lManualLabel, SLOT(setVisible(bool)));
    auto lManualLayout = new QGridLayout;
    lManualLayout->setColumnMinimumWidth(0, lIndentation);
    lManualLayout->setContentsMargins(0, 0, 0, 0);
    lManualLayout->addWidget(lManualLabel, 0, 1);

    auto lIntervalWidget = new QWidget;
    lIntervalWidget->setVisible(false);
    connect(lIntervalRadio, SIGNAL(toggled(bool)), lIntervalWidget, SLOT(setVisible(bool)));
    auto lIntervalLabel = new QLabel(xi18nc("@info",
                                            "New backup will be triggered when backup "
                                            "destination becomes available and more than "
                                            "the configured interval has passed since the "
                                            "last backup was saved."));
    lIntervalLabel->setWordWrap(true);
    auto lIntervalVertLayout = new QGridLayout;
    lIntervalVertLayout->setColumnMinimumWidth(0, lIndentation);
    lIntervalVertLayout->setContentsMargins(0, 0, 0, 0);
    lIntervalVertLayout->addWidget(lIntervalLabel, 0, 1);
    auto lIntervalLayout = new QHBoxLayout;
    lIntervalLayout->setContentsMargins(0, 0, 0, 0);
    auto lIntervalSpinBox = new QSpinBox;
    lIntervalSpinBox->setObjectName(QStringLiteral("kcfg_Schedule interval"));
    lIntervalSpinBox->setMinimum(1);
    lIntervalLayout->addWidget(lIntervalSpinBox);
    auto lIntervalUnit = new KComboBox;
    lIntervalUnit->setObjectName(QStringLiteral("kcfg_Schedule interval unit"));
    lIntervalUnit->addItem(xi18nc("@item:inlistbox", "Minutes"));
    lIntervalUnit->addItem(xi18nc("@item:inlistbox", "Hours"));
    lIntervalUnit->addItem(xi18nc("@item:inlistbox", "Days"));
    lIntervalUnit->addItem(xi18nc("@item:inlistbox", "Weeks"));
    lIntervalLayout->addWidget(lIntervalUnit);
    lIntervalLayout->addStretch();
    lIntervalVertLayout->addLayout(lIntervalLayout, 1, 1);
    lIntervalWidget->setLayout(lIntervalVertLayout);

    auto lUsageWidget = new QWidget;
    lUsageWidget->setVisible(false);
    connect(lUsageRadio, SIGNAL(toggled(bool)), lUsageWidget, SLOT(setVisible(bool)));
    auto lUsageLabel = new QLabel(xi18nc("@info",
                                         "New backup will be triggered when backup destination "
                                         "becomes available and you have been using your "
                                         "computer actively for more than the configured "
                                         "time limit since the last backup was saved."));
    lUsageLabel->setWordWrap(true);
    auto lUsageVertLayout = new QGridLayout;
    lUsageVertLayout->setColumnMinimumWidth(0, lIndentation);
    lUsageVertLayout->setContentsMargins(0, 0, 0, 0);
    lUsageVertLayout->addWidget(lUsageLabel, 0, 1);
    auto lUsageLayout = new QHBoxLayout;
    lUsageLayout->setContentsMargins(0, 0, 0, 0);
    auto lUsageSpinBox = new QSpinBox;
    lUsageSpinBox->setObjectName(QStringLiteral("kcfg_Usage limit"));
    lUsageSpinBox->setMinimum(1);
    lUsageLayout->addWidget(lUsageSpinBox);
    lUsageLayout->addWidget(new QLabel(xi18nc("@item:inlistbox", "Hours")));
    lUsageLayout->addStretch();
    lUsageVertLayout->addLayout(lUsageLayout, 1, 1);
    lUsageWidget->setLayout(lUsageVertLayout);

    auto lAskFirstCheckBox = new QCheckBox(xi18nc("@option:check", "Ask for confirmation before saving backup"));
    lAskFirstCheckBox->setObjectName(QStringLiteral("kcfg_Ask first"));
    connect(lManualRadio, SIGNAL(toggled(bool)), lAskFirstCheckBox, SLOT(setHidden(bool)));

    lVLayout->addWidget(lManualRadio);
    lVLayout->addLayout(lManualLayout);
    lVLayout->addWidget(lIntervalRadio);
    lVLayout->addWidget(lIntervalWidget);
    lVLayout->addWidget(lUsageRadio);
    lVLayout->addWidget(lUsageWidget);
    lButtonGroup->setLayout(lVLayout);

    lTopLayout->addWidget(lButtonGroup);
    lTopLayout->addSpacing(lAskFirstCheckBox->fontMetrics().height());
    lTopLayout->addWidget(lAskFirstCheckBox);
    lTopLayout->addStretch();
    lTopWidget->setLayout(lTopLayout);

    auto lPage = new KPageWidgetItem(lTopWidget);
    lPage->setName(xi18nc("@title", "Schedule"));
    lPage->setHeader(xi18nc("@label", "Specify the backup schedule"));
    lPage->setIcon(QIcon::fromTheme(QStringLiteral("view-calendar")));
    return lPage;
}

KPageWidgetItem *BackupPlanWidget::createAdvancedPage(bool pPar2Available)
{
    auto lAdvancedWidget = new QWidget(this);
    auto lAdvancedLayout = new QVBoxLayout;

    int lIndentation =
        lAdvancedWidget->style()->pixelMetric(QStyle::PM_IndicatorWidth) + lAdvancedWidget->style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing);

    auto lShowHiddenWidget = new QWidget;
    auto lShowHiddenCheckBox = new QCheckBox(xi18nc("@option:check", "Show hidden folders in source selection"));
    lShowHiddenCheckBox->setObjectName(QStringLiteral("kcfg_Show hidden folders"));
    connect(lShowHiddenCheckBox, SIGNAL(toggled(bool)), mSourceSelectionWidget, SLOT(setHiddenFoldersVisible(bool)));
    auto lShowHiddenLabel = new QLabel(xi18nc("@info",
                                              "This makes it possible to explicitly include or "
                                              "exclude hidden folders in the backup source "
                                              "selection. Hidden folders have a name that starts "
                                              "with a dot. They are typically located in your home "
                                              "folder and are used to store settings and temporary "
                                              "files for your applications."));
    lShowHiddenLabel->setWordWrap(true);
    auto lShowHiddenLayout = new QGridLayout;
    lShowHiddenLayout->setContentsMargins(0, 0, 0, 0);
    lShowHiddenLayout->setSpacing(0);
    lShowHiddenLayout->setColumnMinimumWidth(0, lIndentation);
    lShowHiddenLayout->addWidget(lShowHiddenCheckBox, 0, 0, 1, 2);
    lShowHiddenLayout->addWidget(lShowHiddenLabel, 1, 1);
    lShowHiddenWidget->setLayout(lShowHiddenLayout);

    auto lRecoveryWidget = new QWidget;
    lRecoveryWidget->setVisible(false);
    auto lRecoveryCheckBox = new QCheckBox;
    lRecoveryCheckBox->setObjectName(QStringLiteral("kcfg_Generate recovery info"));

    auto lRecoveryLabel = new QLabel(xi18nc("@info",
                                            "This will make your backups use around 10% more storage "
                                            "space and saving backups will take slightly longer time. In "
                                            "return it will be possible to recover from a partially corrupted "
                                            "backup."));
    lRecoveryLabel->setWordWrap(true);
    if (pPar2Available) {
        lRecoveryCheckBox->setText(xi18nc("@option:check", "Generate recovery information"));
    } else {
        lRecoveryCheckBox->setText(xi18nc("@option:check",
                                          "Generate recovery information (not available "
                                          "because <application>par2</application> is not installed)"));
        lRecoveryCheckBox->setEnabled(false);
        lRecoveryLabel->setEnabled(false);
    }
    auto lRecoveryLayout = new QGridLayout;
    lRecoveryLayout->setContentsMargins(0, 0, 0, 0);
    lRecoveryLayout->setSpacing(0);
    lRecoveryLayout->setColumnMinimumWidth(0, lIndentation);
    lRecoveryLayout->addWidget(lRecoveryCheckBox, 0, 0, 1, 2);
    lRecoveryLayout->addWidget(lRecoveryLabel, 1, 1);
    lRecoveryWidget->setLayout(lRecoveryLayout);
    connect(mVersionedRadio, SIGNAL(toggled(bool)), lRecoveryWidget, SLOT(setVisible(bool)));

    auto lVerificationWidget = new QWidget;
    lVerificationWidget->setVisible(false);
    auto lVerificationCheckBox = new QCheckBox(xi18nc("@option:check", "Verify integrity of backups"));
    lVerificationCheckBox->setObjectName(QStringLiteral("kcfg_Check backups"));

    auto lVerificationLabel = new QLabel(xi18nc("@info",
                                                "Checks the whole backup archive for corruption "
                                                "every time you save new data. Saving backups will take a "
                                                "little bit longer time but it allows you to catch corruption "
                                                "problems sooner than at the time you need to use a backup, "
                                                "at that time it could be too late."));
    lVerificationLabel->setWordWrap(true);
    auto lVerificationLayout = new QGridLayout;
    lVerificationLayout->setContentsMargins(0, 0, 0, 0);
    lVerificationLayout->setSpacing(0);
    lVerificationLayout->setColumnMinimumWidth(0, lIndentation);
    lVerificationLayout->addWidget(lVerificationCheckBox, 0, 0, 1, 2);
    lVerificationLayout->addWidget(lVerificationLabel, 1, 1);
    lVerificationWidget->setLayout(lVerificationLayout);
    connect(mVersionedRadio, SIGNAL(toggled(bool)), lVerificationWidget, SLOT(setVisible(bool)));

    auto lExcludesWidget = new QWidget;
    auto lExcludesCheckBox = new QCheckBox(xi18nc("@option:check", "Exclude files and folders based on patterns"));
    lExcludesCheckBox->setObjectName(QStringLiteral("kcfg_Exclude patterns"));

    auto lExcludesLabel = new QLabel();
    lExcludesLabel->setWordWrap(true);
    lExcludesLabel->setTextFormat(Qt::RichText);
    lExcludesLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    connect(lExcludesLabel, &QLabel::linkActivated, this, [](QString pLink) {
        // open the URL ourselves instead of QLabel and QDesktopService doing it, just
        // so that we can give a .html file ending to the temp file. Required for QWebEngine
        // to understand that the file has html content. Firefox works either way.
        auto *job = new KIO::OpenUrlJob(QUrl(pLink));
        job->setSuggestedFileName("manpage.html");
        job->start();
    });
    auto lLabelUpdater = [lExcludesLabel](bool pVersioned) {
        QString lHelpUrl = pVersioned ? QStringLiteral("man:///bup-index") : QStringLiteral("man:///rsync");
        lExcludesLabel->setText(xi18nc("@info",
                                       "Patterns need to be listed in a text file with one pattern per line. "
                                       "Files and folders with names that match any of the patterns will be "
                                       "excluded from the backup. The pattern format is documented <link url='%1'>here</link>.",
                                       lHelpUrl));
    };
    lLabelUpdater(false);
    connect(mVersionedRadio, &QCheckBox::toggled, this, lLabelUpdater);
    auto lExcludesEdit = new KLineEdit;
    lExcludesEdit->setObjectName(QStringLiteral("kcfg_Exclude patterns file path"));
    lExcludesEdit->setEnabled(false);
    lExcludesEdit->setClearButtonEnabled(true);
    lExcludesEdit->setCompletionObject(new KUrlCompletion);
    lExcludesEdit->setAutoDeleteCompletionObject(true);
    auto lExcludesButton = new QPushButton;
    lExcludesButton->setIcon(QIcon::fromTheme(QStringLiteral("document-open")));
    int lButtonSize = lExcludesButton->sizeHint().expandedTo(lExcludesEdit->sizeHint()).height();
    lExcludesButton->setFixedSize(lButtonSize, lButtonSize);
    lExcludesButton->setEnabled(false);
    lExcludesButton->setToolTip(xi18nc("@info:tooltip", "Open dialog to select a file"));
    connect(lExcludesButton, &QPushButton::clicked, this, [this, lExcludesEdit] {
        QString lNewFilePath = QFileDialog::getOpenFileName(this, i18n("Select pattern file"), lExcludesEdit->text());
        if (!lNewFilePath.isEmpty()) {
            lExcludesEdit->setText(lNewFilePath);
        }
    });
    connect(lExcludesCheckBox, &QCheckBox::toggled, lExcludesEdit, &KLineEdit::setEnabled);
    connect(lExcludesCheckBox, &QCheckBox::toggled, lExcludesButton, &QPushButton::setEnabled);
    auto lExcludesLayout = new QGridLayout;
    lExcludesLayout->setContentsMargins(0, 0, 0, 0);
    lExcludesLayout->setSpacing(0);
    lExcludesLayout->setColumnMinimumWidth(0, lIndentation);
    lExcludesLayout->addWidget(lExcludesCheckBox, 0, 0, 1, 3);
    lExcludesLayout->addWidget(lExcludesLabel, 1, 1, 1, 2);
    lExcludesLayout->addWidget(lExcludesEdit, 2, 1);
    lExcludesLayout->addWidget(lExcludesButton, 2, 2);
    lExcludesWidget->setLayout(lExcludesLayout);

    lAdvancedLayout->addWidget(lShowHiddenWidget);
    lAdvancedLayout->addWidget(lVerificationWidget);
    lAdvancedLayout->addWidget(lRecoveryWidget);
    lAdvancedLayout->addWidget(lExcludesWidget);
    lAdvancedLayout->addStretch();
    lAdvancedLayout->setSpacing(lIndentation);
    lAdvancedWidget->setLayout(lAdvancedLayout);
    auto lPage = new KPageWidgetItem(lAdvancedWidget);
    lPage->setName(xi18nc("@title", "Advanced"));
    lPage->setHeader(xi18nc("@label", "Extra options for advanced users"));
    lPage->setIcon(QIcon::fromTheme(QStringLiteral("preferences-other")));
    return lPage;
}

void BackupPlanWidget::openDriveDestDialog()
{
    QString lMountPoint = mDriveSelection->mountPathOfSelectedDrive();
    QPointer<DirDialog> lDirDialog = new DirDialog(QUrl::fromLocalFile(lMountPoint), mDriveDestEdit->text(), this);
    if (lDirDialog->exec() == QDialog::Accepted) {
        QString lSelectedPath = lDirDialog->url().path();
        lSelectedPath.remove(0, lMountPoint.length());
        while (lSelectedPath.startsWith(QLatin1Char('/'))) {
            lSelectedPath.remove(0, 1);
        }
        mDriveDestEdit->setText(lSelectedPath);
    }
    delete lDirDialog;
}

void BackupPlanWidget::checkFilesystemDestination(const QString &pDestination)
{
    if (!pDestination.startsWith("/") && !pDestination.startsWith("file:") && pDestination.contains(":/")) {
        mLocalMessage->animatedShow();
    } else {
        mLocalMessage->animatedHide();
    }

    QDir lDestinationDir(QDir::home().absoluteFilePath(pDestination));
    if (!lDestinationDir.exists()) {
        auto lAction = new QAction(xi18nc("@action:button", "Create Folder"), this);
        connect(lAction, &QAction::triggered, this, [this, lDestinationDir]() {
            lDestinationDir.mkpath(lDestinationDir.absolutePath());
            checkFilesystemDestination(lDestinationDir.absolutePath());
        });
        mExistMessage->clearActions();
        mExistMessage->addAction(lAction);
        if (mExistMessage->isHidden()) {
            mExistMessage->animatedShow();
        } else {
            // Work around for buggy layout when removing and adding action.
            mExistMessage->hide();
            mExistMessage->show();
        }
    } else {
        mExistMessage->animatedHide();
    }
}
