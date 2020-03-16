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

	const VersionList *versionList(const QModelIndex &pIndex);
	const MergedNode *node(const QModelIndex &pIndex);

protected:
	MergedRepository *mRoot;

};

#endif // MERGEDVFSMODEL_H
