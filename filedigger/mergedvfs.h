// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef MERGEDVFS_H
#define MERGEDVFS_H

#include <git2.h>
uint qHash(git_oid pOid);
bool operator==(const git_oid &pOidA, const git_oid &pOidB);
#include <QHash>
#include <QObject>

#include <QUrl>

#include <sys/stat.h>

struct VersionData {
    VersionData(bool pChunkedFile, const git_oid *pOid, qint64 pCommitTime, qint64 pModifiedDate)
        : mChunkedFile(pChunkedFile)
        , mOid(*pOid)
        , mCommitTime(pCommitTime)
        , mModifiedDate(pModifiedDate)
    {
        mSizeIsValid = false;
    }

    VersionData(const git_oid *pOid, qint64 pCommitTime, qint64 pModifiedDate, quint64 pSize)
        : mChunkedFile(false)
        , mOid(*pOid)
        , mCommitTime(pCommitTime)
        , mModifiedDate(pModifiedDate)
        , mSize(pSize)
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
    quint64 mSize{};
};

class MergedNode;
typedef QList<MergedNode *> MergedNodeList;
typedef QListIterator<MergedNode *> MergedNodeListIterator;
typedef QList<VersionData *> VersionList;
typedef QListIterator<VersionData *> VersionListIterator;

class MergedNode : public QObject
{
    Q_OBJECT
    friend struct VersionData;

public:
    MergedNode(QObject *pParent, const QString &pName, uint pMode);
    ~MergedNode() override
    {
        delete mSubNodes;
        while (!mVersionList.isEmpty())
            delete mVersionList.takeFirst();
    }
    bool isDirectory() const
    {
        return S_ISDIR(mMode);
    }
    void getBupUrl(int pVersionIndex,
                   QUrl *pComplete,
                   QString *pRepoPath = nullptr,
                   QString *pBranchName = nullptr,
                   qint64 *pCommitTime = nullptr,
                   QString *pPathInRepo = nullptr) const;
    virtual MergedNodeList &subNodes();
    const VersionList *versionList() const
    {
        return &mVersionList;
    }
    uint mode() const
    {
        return mMode;
    }
    static void askForIntegrityCheck();

protected:
    virtual void generateSubNodes();

    static git_repository *mRepository;
    uint mMode;
    VersionList mVersionList;
    MergedNodeList *mSubNodes;
};

class MergedRepository : public MergedNode
{
    Q_OBJECT
public:
    MergedRepository(QObject *pParent, const QString &pRepositoryPath, QString pBranchName);
    ~MergedRepository() override;

    bool open();
    bool readBranch();
    bool permissionsOk();

    QString mBranchName;
};

#endif // MERGEDVFS_H
