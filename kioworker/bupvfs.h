// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef BUPVFS_H
#define BUPVFS_H

#include <QHash>
#include <QObject>
#include <kio/global.h>
#include <sys/types.h>

#include "vfshelpers.h"

class Node : public QObject, public Metadata
{
    Q_OBJECT
public:
    Node(QObject *pParent, const QString &pName, qint64 pMode);
    ~Node() override
    {
    }
    virtual int readMetadata(VintStream &pMetadataStream);
    Node *resolve(const QString &pPath, bool pFollowLinks = false);
    Node *resolve(const QStringList &pPathList, bool pFollowLinks = false);
    QString completePath();
    Node *parentCommit();
    //	Node *parentRepository();
    QString mMimeType;

protected:
    static git_revwalk *mRevisionWalker;
    static git_repository *mRepository;
};

typedef QHash<QString, Node *> NodeMap;
typedef QHashIterator<QString, Node *> NodeMapIterator;

class Directory : public Node
{
    Q_OBJECT
public:
    Directory(QObject *pParent, const QString &pName, qint64 pMode);
    ~Directory() override
    {
        delete mSubNodes;
    }
    virtual NodeMap subNodes();
    virtual void reload()
    {
    }

protected:
    virtual void generateSubNodes()
    {
    }
    NodeMap *mSubNodes;
};

class File : public Node
{
    Q_OBJECT
public:
    File(QObject *pParent, const QString &pName, qint64 pMode)
        : Node(pParent, pName, pMode)
    {
        mOffset = 0;
        mCachedSize = 0;
    }
    virtual quint64 size()
    {
        if (mCachedSize == 0) {
            mCachedSize = calculateSize();
        }
        return mCachedSize;
    }
    virtual int seek(quint64 pOffset)
    {
        if (pOffset >= size()) {
            return KIO::ERR_CANNOT_SEEK;
        }
        mOffset = pOffset;
        return 0; // success
    }
    virtual int read(QByteArray &pChunk, qint64 pReadSize = -1) = 0;
    int readMetadata(VintStream &pMetadataStream) override;

protected:
    virtual quint64 calculateSize() = 0;
    quint64 mOffset;
    quint64 mCachedSize;
};

class BlobFile : public File
{
    Q_OBJECT
public:
    BlobFile(Node *pParent, const git_oid *pOid, const QString &pName, qint64 pMode);
    ~BlobFile() override;
    int read(QByteArray &pChunk, qint64 pReadSize = -1) override;

protected:
    git_blob *cachedBlob();
    quint64 calculateSize() override;
    git_oid mOid{};
    git_blob *mBlob;
};

class Symlink : public BlobFile
{
    Q_OBJECT
public:
    Symlink(Node *pParent, const git_oid *pOid, const QString &pName, qint64 pMode)
        : BlobFile(pParent, pOid, pName, pMode)
    {
        QByteArray lArray;
        if (0 == read(lArray)) {
            mSymlinkTarget = QString::fromUtf8(lArray.data(), lArray.size());
            Symlink::seek(0);
        }
    }
};

class ChunkFile : public File
{
    Q_OBJECT
public:
    ChunkFile(Node *pParent, const git_oid *pOid, const QString &pName, qint64 pMode);
    ~ChunkFile() override;
    int seek(quint64 pOffset) override;
    int read(QByteArray &pChunk, qint64 pReadSize = -1) override;

protected:
    quint64 calculateSize() override;

    git_oid mOid;
    git_blob *mCurrentBlob;
    struct TreePosition {
        explicit TreePosition(git_tree *pTree);
        ~TreePosition();
        git_tree *mTree;
        ulong mIndex;
        quint64 mSkipSize;
    };

    QList<TreePosition *> mPositionStack;
    bool mValidSeekPosition;
};

class ArchivedDirectory : public Directory
{
    Q_OBJECT
public:
    ArchivedDirectory(Node *pParent, const git_oid *pOid, const QString &pName, qint64 pMode);

protected:
    void generateSubNodes() override;
    git_oid mOid{};
    git_blob *mMetadataBlob{};
    git_tree *mTree;
    VintStream *mMetadataStream;
};

class Branch : public Directory
{
    Q_OBJECT
public:
    Branch(Node *pParent, const char *pName);
    void reload() override;

protected:
    void generateSubNodes() override;
    QByteArray mRefName;
};

class Repository : public Directory
{
    Q_OBJECT
public:
    Repository(QObject *pParent, const QString &pRepositoryPath);
    ~Repository() override;
    bool isValid()
    {
        return mRepository != nullptr && mRevisionWalker != nullptr;
    }

protected:
    void generateSubNodes() override;
};

#endif // BUPVFS_H
