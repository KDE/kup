// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef MERGEDVFSMODEL_H
#define MERGEDVFSMODEL_H

#include <QAbstractItemModel>

#include "mergedvfs.h"

class MergedVfsModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit MergedVfsModel(MergedRepository *pRoot, QObject *pParent = nullptr);
    ~MergedVfsModel() override;
    int columnCount(const QModelIndex &pParent) const override;
    QVariant data(const QModelIndex &pIndex, int pRole) const override;
    QModelIndex index(int pRow, int pColumn, const QModelIndex &pParent) const override;
    QModelIndex parent(const QModelIndex &pChild) const override;
    int rowCount(const QModelIndex &pParent) const override;

    static const VersionList *versionList(const QModelIndex &pIndex);
    static const MergedNode *node(const QModelIndex &pIndex);

protected:
    MergedRepository *mRoot;
};

#endif // MERGEDVFSMODEL_H
