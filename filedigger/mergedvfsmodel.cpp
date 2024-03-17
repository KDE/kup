// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "mergedvfsmodel.h"
#include "mergedvfs.h"
#include "vfshelpers.h"

#include <KIO/Global>

#include <QIcon>
#include <QMimeDatabase>
#include <QMimeType>
#include <QPixmap>

MergedVfsModel::MergedVfsModel(MergedRepository *pRoot, QObject *pParent)
    : QAbstractItemModel(pParent)
    , mRoot(pRoot)
{
}

MergedVfsModel::~MergedVfsModel()
{
    delete mRoot;
}

int MergedVfsModel::columnCount(const QModelIndex &pParent) const
{
    Q_UNUSED(pParent)
    return 1;
}

QVariant MergedVfsModel::data(const QModelIndex &pIndex, int pRole) const
{
    if (!pIndex.isValid()) {
        return QVariant();
    }
    auto *lNode = static_cast<MergedNode *>(pIndex.internalPointer());
    switch (pRole) {
    case Qt::DisplayRole:
        return lNode->objectName();
    case Qt::DecorationRole: {
        QString lIconName = KIO::iconNameForUrl(QUrl::fromLocalFile(lNode->objectName()));
        if (lNode->isDirectory()) {
            QMimeDatabase db;
            lIconName = db.mimeTypeForName(QStringLiteral("inode/directory")).iconName();
        }
        return QIcon::fromTheme(lIconName);
    }
    default:
        return QVariant();
    }
}

QModelIndex MergedVfsModel::index(int pRow, int pColumn, const QModelIndex &pParent) const
{
    if (pColumn != 0 || pRow < 0) {
        return {}; // invalid
    }
    if (!pParent.isValid()) {
        if (pRow >= mRoot->subNodes().count()) {
            return {}; // invalid
        }
        return createIndex(pRow, 0, mRoot->subNodes().at(pRow));
    }
    auto lParentNode = static_cast<MergedNode *>(pParent.internalPointer());
    if (pRow >= lParentNode->subNodes().count()) {
        return {}; // invalid
    }
    return createIndex(pRow, 0, lParentNode->subNodes().at(pRow));
}

QModelIndex MergedVfsModel::parent(const QModelIndex &pChild) const
{
    if (!pChild.isValid()) {
        return {};
    }
    auto lChild = static_cast<MergedNode *>(pChild.internalPointer());
    auto lParent = qobject_cast<MergedNode *>(lChild->parent());
    if (lParent == nullptr || lParent == mRoot) {
        return {}; // invalid
    }
    auto lGrandParent = qobject_cast<MergedNode *>(lParent->parent());
    if (lGrandParent == nullptr) {
        return {}; // invalid
    }
    return createIndex(lGrandParent->subNodes().indexOf(lParent), 0, lParent);
}

int MergedVfsModel::rowCount(const QModelIndex &pParent) const
{
    if (!pParent.isValid()) {
        return mRoot->subNodes().count();
    }
    auto lParent = static_cast<MergedNode *>(pParent.internalPointer());
    if (lParent == nullptr) {
        return 0;
    }
    return lParent->subNodes().count();
}

const VersionList *MergedVfsModel::versionList(const QModelIndex &pIndex)
{
    auto lNode = static_cast<MergedNode *>(pIndex.internalPointer());
    return lNode->versionList();
}

const MergedNode *MergedVfsModel::node(const QModelIndex &pIndex)
{
    return static_cast<MergedNode *>(pIndex.internalPointer());
}
