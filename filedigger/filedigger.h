// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef FILEDIGGER_H
#define FILEDIGGER_H

#include <KMainWindow>
#include <QUrl>

class KDirOperator;
class MergedVfsModel;
class MergedRepository;
class VersionListModel;
class QListView;
class QModelIndex;
class QTreeView;

class FileDigger : public KMainWindow
{
    Q_OBJECT
public:
    explicit FileDigger(QString pRepoPath, QString pBranchName, QWidget *pParent = nullptr);
    QSize sizeHint() const override;

protected slots:
    void updateVersionModel(const QModelIndex &pCurrent, const QModelIndex &pPrevious);
    void open(const QModelIndex &pIndex);
    void restore(const QModelIndex &pIndex);
    void repoPathAvailable();
    void checkFileWidgetPath();
    void enterUrl(const QUrl &pUrl);

protected:
    MergedRepository *createRepo();
    void createRepoView(MergedRepository *pRepository);
    void createSelectionView();
    MergedVfsModel *mMergedVfsModel{};
    QTreeView *mMergedVfsView{};

    VersionListModel *mVersionModel{};
    QListView *mVersionView{};
    QString mRepoPath;
    QString mBranchName;
    KDirOperator *mDirOperator;
};

#endif // FILEDIGGER_H
