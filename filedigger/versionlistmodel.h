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

#ifndef VERSIONLISTMODEL_H
#define VERSIONLISTMODEL_H

#include <QAbstractListModel>
#include "mergedvfs.h"

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
	const MergedNode *mNode;
};

enum VersionDataRole {
	VersionBupUrlRole = Qt::UserRole + 1, // QUrl
	VersionMimeTypeRole, // QString
	VersionSizeRole, // quint64
	VersionSourceInfoRole, // PathInfo
	VersionIsDirectoryRole // bool
};

#endif // VERSIONLISTMODEL_H
