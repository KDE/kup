// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "versionlistmodel.h"
#include "vfshelpers.h"

#include <KFormat>
#include <KLocalizedString>

#include <QDateTime>
#include <QLocale>
#include <QMimeDatabase>
#include <QMimeType>

VersionListModel::VersionListModel(QObject *parent)
    : QAbstractListModel(parent)
{
    mVersionList = nullptr;
}

void VersionListModel::setNode(const MergedNode *pNode)
{
    beginResetModel();
    mNode = pNode;
    mVersionList = mNode->versionList();
    endResetModel();
}

int VersionListModel::rowCount(const QModelIndex &pParent) const
{
    Q_UNUSED(pParent)
    if (mVersionList != nullptr) {
        return mVersionList->count();
    }
    return 0;
}

QVariant VersionListModel::data(const QModelIndex &pIndex, int pRole) const
{
    if (!pIndex.isValid() || mVersionList == nullptr) {
        return QVariant();
    }
    QMimeDatabase db;
    KFormat lFormat;
    VersionData *lData = mVersionList->at(pIndex.row());
    switch (pRole) {
    case Qt::DisplayRole:
        return lFormat.formatRelativeDateTime(QDateTime::fromSecsSinceEpoch(static_cast<qint64>(lData->mModifiedDate)), QLocale::ShortFormat);
    case VersionBupUrlRole: {
        QUrl lUrl;
        mNode->getBupUrl(pIndex.row(), &lUrl);
        return lUrl;
    }
    case VersionMimeTypeRole:
        if (mNode->isDirectory()) {
            return QString(QStringLiteral("inode/directory"));
        }
        return db.mimeTypeForFile(mNode->objectName(), QMimeDatabase::MatchExtension).name();
    case VersionSizeRole:
        return lData->size();
    case VersionSourceInfoRole: {
        BupSourceInfo lSourceInfo;
        mNode->getBupUrl(pIndex.row(),
                         &lSourceInfo.mBupKioPath,
                         &lSourceInfo.mRepoPath,
                         &lSourceInfo.mBranchName,
                         &lSourceInfo.mCommitTime,
                         &lSourceInfo.mPathInRepo);
        lSourceInfo.mIsDirectory = mNode->isDirectory();
        lSourceInfo.mSize = lData->size();
        return QVariant::fromValue<BupSourceInfo>(lSourceInfo);
    }
    case VersionIsDirectoryRole:
        return mNode->isDirectory();
    default:
        return QVariant();
    }
}
