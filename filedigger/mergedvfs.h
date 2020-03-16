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

#ifndef MERGEDVFS_H
#define MERGEDVFS_H

#include <git2.h>
uint qHash(git_oid pOid);
bool operator ==(const git_oid &pOidA, const git_oid &pOidB);
#include <QHash>
#include <QObject>

#include <QUrl>

#include <sys/stat.h>

struct VersionData {
	VersionData(bool pChunkedFile, const git_oid *pOid, qint64 pCommitTime, qint64 pModifiedDate)
	   :mChunkedFile(pChunkedFile), mOid(*pOid), mCommitTime(pCommitTime), mModifiedDate(pModifiedDate)
	{
		mSizeIsValid = false;
	}

	VersionData(const git_oid *pOid, qint64 pCommitTime, qint64 pModifiedDate, quint64 pSize)
	   :mOid(*pOid), mCommitTime(pCommitTime), mModifiedDate(pModifiedDate), mSize(pSize)
	{
		mSizeIsValid = true;
	}

	quint64 size();
	bool mSizeIsValid;
	bool mChunkedFile;
	git_oid mOid;
	qint64 mCommitTime;
	qint64 mModifiedDate;

protected:
	quint64 mSize;
};

class MergedNode;
typedef QList<MergedNode*> MergedNodeList;
typedef QListIterator<MergedNode*> MergedNodeListIterator;
typedef QList<VersionData *> VersionList;
typedef QListIterator<VersionData *> VersionListIterator;

class MergedNode: public QObject {
	Q_OBJECT
	friend struct VersionData;
public:
	MergedNode(QObject *pParent, const QString &pName, uint pMode);
	~MergedNode() override {
		if(mSubNodes != nullptr) {
			delete mSubNodes;
		}
	}
	bool isDirectory() const { return S_ISDIR(mMode); }
	void getBupUrl(int pVersionIndex, QUrl *pComplete, QString *pRepoPath = nullptr, QString *pBranchName = nullptr,
	               qint64 *pCommitTime = nullptr, QString *pPathInRepo = nullptr) const;
	virtual MergedNodeList &subNodes();
	const VersionList *versionList() const { return &mVersionList; }
	uint mode() const { return mMode; }
	static void askForIntegrityCheck();

protected:
	virtual void generateSubNodes();

	static git_repository *mRepository;
	uint mMode;
	VersionList mVersionList;
	MergedNodeList *mSubNodes;
};

class MergedRepository: public MergedNode {
	Q_OBJECT
public:
	MergedRepository(QObject *pParent, const QString &pRepositoryPath, const QString &pBranchName);
	~MergedRepository() override;

	bool open();
	bool readBranch();
	bool permissionsOk();

	QString mBranchName;
};

#endif // MERGEDVFS_H
