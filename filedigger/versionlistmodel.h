// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef VERSIONLISTMODEL_H
#define VERSIONLISTMODEL_H

#include "mergedvfs.h"
#include <QAbstractListModel>

struct BupSourceInfo {
    QUrl mBupKioPath;
    QString mRepoPath;
    QString mBranchName;
    QString mPathInRepo;
    qint64 mCommitTime;
    quint64 mSize;
    bool mIsDirectory;
};

Q_DECLARE_METATYPE(BupSourceInfo)

class VersionListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit VersionListModel(QObject *parent = nullptr);
    void setNode(const MergedNode *pNode);
    int rowCount(const QModelIndex &pParent) const override;
    QVariant data(const QModelIndex &pIndex, int pRole) const override;

protected:
    const VersionList *mVersionList;
    const MergedNode *mNode{};
};

enum VersionDataRole {
    VersionBupUrlRole = Qt::UserRole + 1, // QUrl
    VersionMimeTypeRole, // QString
    VersionSizeRole, // quint64
    VersionSourceInfoRole, // PathInfo
    VersionIsDirectoryRole // bool
};

#endif // VERSIONLISTMODEL_H
