// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "bupvfs.h"
#include "kupkio_debug.h"

#include <git2/blob.h>
#include <git2/branch.h>

#include <sys/stat.h>

#include <QMimeDatabase>

git_revwalk *Node::mRevisionWalker = nullptr;
git_repository *Node::mRepository = nullptr;

Node::Node(QObject *pParent, const QString &pName, qint64 pMode)
    : QObject(pParent)
    , Metadata(pMode)
{
    setObjectName(pName);
}

int Node::readMetadata(VintStream &pMetadataStream)
{
    return ::readMetadata(pMetadataStream, *this);
}

Node *Node::resolve(const QString &pPath, bool pFollowLinks)
{
    Node *lParentNode = this;
    QString lTarget = pPath;
    if (lTarget.startsWith(QLatin1Char('/'))) {
        lTarget.remove(0, 1);
        lParentNode = parentCommit();
    }
    return lParentNode->resolve(lTarget.split(QLatin1Char('/'), Qt::SkipEmptyParts), pFollowLinks);
}

Node *Node::resolve(const QStringList &pPathList, bool pFollowLinks)
{
    Node *lNode = this;
    foreach (QString lPathComponent, pPathList) {
        if (lPathComponent == QStringLiteral(".")) {
            continue;
        }
        if (lPathComponent == QStringLiteral("..")) {
            lNode = qobject_cast<Node *>(lNode->parent());
        } else {
            auto lDir = qobject_cast<Directory *>(lNode);
            if (lDir == nullptr) {
                return nullptr;
            }
            lNode = lDir->subNodes().value(lPathComponent, nullptr);
        }
        if (lNode == nullptr) {
            return nullptr;
        }
    }
    if (pFollowLinks && !lNode->mSymlinkTarget.isEmpty()) {
        return qobject_cast<Node *>(lNode->parent())->resolve(lNode->mSymlinkTarget, true);
    }
    return lNode;
}

QString Node::completePath()
{
    QString lCompletePath;
    Node *lNode = this;
    while (lNode != nullptr) {
        Node *lNewNode = qobject_cast<Node *>(lNode->parent());
        if (lNewNode == nullptr) { // this must be the repository, already starts and ends with slash.
            QString lObjectName = lNode->objectName();
            lObjectName.chop(1);
            lCompletePath.prepend(lObjectName);
        } else {
            lCompletePath.prepend(lNode->objectName());
            lCompletePath.prepend(QStringLiteral("/"));
        }
        lNode = lNewNode;
    }
    return lCompletePath;
}

Node *Node::parentCommit()
{
    Node *lNode = this;
    while (lNode != nullptr && qobject_cast<Branch *>(lNode->parent()) == nullptr) {
        lNode = qobject_cast<Node *>(lNode->parent());
    }
    return lNode;
}

// Node *Node::parentRepository() {
//	Node *lNode = this;
//	while(lNode->parent() != nullptr && qobject_cast<Repository *>(lNode) == nullptr) {
//		lNode = qobject_cast<Node *>(lNode->parent());
//	}
//	return lNode;
// }

Directory::Directory(QObject *pParent, const QString &pName, qint64 pMode)
    : Node(pParent, pName, pMode)
{
    mSubNodes = nullptr;
    mMimeType = QStringLiteral("inode/directory");
}

NodeMap Directory::subNodes()
{
    if (mSubNodes == nullptr) {
        mSubNodes = new NodeMap();
        generateSubNodes();
    }
    return *mSubNodes;
}

int File::readMetadata(VintStream &pMetadataStream)
{
    int lRetVal = Node::readMetadata(pMetadataStream);
    QByteArray lContent, lNextData;
    seek(0);
    while (lContent.size() < 1000 && 0 == read(lNextData)) {
        lContent.append(lNextData);
    }
    seek(0);
    QMimeDatabase db;
    if (!lContent.isEmpty()) {
        mMimeType = db.mimeTypeForFileNameAndData(objectName(), lContent).name();
    } else {
        mMimeType = db.mimeTypeForFile(objectName()).name();
    }
    return lRetVal;
}

BlobFile::BlobFile(Node *pParent, const git_oid *pOid, const QString &pName, qint64 pMode)
    : File(pParent, pName, pMode)
    , mOid(*pOid)
    , mBlob(nullptr)
{
}

BlobFile::~BlobFile()
{
    git_blob_free(mBlob);
}

int BlobFile::read(QByteArray &pChunk, qint64 pReadSize)
{
    if (mOffset >= size()) {
        return KIO::ERR_NO_CONTENT;
    }
    git_blob *lBlob = cachedBlob();
    if (lBlob == nullptr) {
        return KIO::ERR_CANNOT_READ;
    }
    quint64 lAvailableSize = size() - mOffset;
    quint64 lReadSize = lAvailableSize;
    if (pReadSize > 0 && static_cast<quint64>(pReadSize) < lAvailableSize) {
        lReadSize = static_cast<quint64>(pReadSize);
    }
    pChunk = QByteArray::fromRawData(static_cast<const char *>(git_blob_rawcontent(lBlob)) + mOffset, static_cast<int>(lReadSize));
    mOffset += lReadSize;
    return 0;
}

git_blob *BlobFile::cachedBlob()
{
    if (mBlob == nullptr) {
        git_blob_lookup(&mBlob, mRepository, &mOid);
    }
    return mBlob;
}

quint64 BlobFile::calculateSize()
{
    if (mSize >= 0) {
        return static_cast<quint64>(mSize);
    }
    git_blob *lBlob = cachedBlob();
    if (lBlob == nullptr) {
        return 0;
    }
    return static_cast<quint64>(git_blob_rawsize(lBlob));
}

ChunkFile::ChunkFile(Node *pParent, const git_oid *pOid, const QString &pName, qint64 pMode)
    : File(pParent, pName, pMode)
    , mOid(*pOid)
    , mCurrentBlob(nullptr)
    , mValidSeekPosition(false)
{
    ChunkFile::seek(0);
}

ChunkFile::~ChunkFile()
{
    if (mCurrentBlob != nullptr) {
        git_blob_free(mCurrentBlob);
    }
}

int ChunkFile::seek(quint64 pOffset)
{
    if (pOffset >= size()) {
        return KIO::ERR_CANNOT_SEEK;
    }
    if (mOffset == pOffset && mValidSeekPosition) {
        return 0; // nothing to do, success
    }

    mOffset = pOffset;
    mValidSeekPosition = false;

    while (!mPositionStack.isEmpty()) {
        delete mPositionStack.takeLast();
    }
    if (mCurrentBlob != nullptr) {
        git_blob_free(mCurrentBlob);
        mCurrentBlob = nullptr;
    }

    git_tree *lTree;
    if (0 != git_tree_lookup(&lTree, mRepository, &mOid)) {
        return KIO::ERR_CANNOT_SEEK;
    }

    auto lCurrentPos = new TreePosition(lTree);
    mPositionStack.append(lCurrentPos);
    quint64 lLocalOffset = mOffset;
    while (true) {
        ulong lLower = 0;
        const git_tree_entry *lLowerEntry = git_tree_entry_byindex(lCurrentPos->mTree, lLower);
        ulong lLowerOffset = 0;
        ulong lUpper = git_tree_entrycount(lCurrentPos->mTree);

        while (lUpper - lLower > 1) {
            ulong lToCheck = lLower + (lUpper - lLower) / 2;
            const git_tree_entry *lCheckEntry = git_tree_entry_byindex(lCurrentPos->mTree, lToCheck);
            quint64 lCheckOffset;
            if (!offsetFromName(lCheckEntry, lCheckOffset)) {
                return KIO::ERR_CANNOT_SEEK;
            }
            if (lCheckOffset > lLocalOffset) {
                lUpper = lToCheck;
            } else {
                lLower = lToCheck;
                lLowerEntry = lCheckEntry;
                lLowerOffset = lCheckOffset;
            }
        }
        lCurrentPos->mIndex = lLower;
        // the remainder of the offset will be a local offset into the blob or into the subtree.
        lLocalOffset -= lLowerOffset;

        if (S_ISDIR(git_tree_entry_filemode(lLowerEntry))) {
            git_tree *lLowerTree;
            if (0 != git_tree_lookup(&lLowerTree, mRepository, git_tree_entry_id(lLowerEntry))) {
                return KIO::ERR_CANNOT_SEEK;
            }
            lCurrentPos = new TreePosition(lLowerTree);
            mPositionStack.append(lCurrentPos);
        } else {
            lCurrentPos->mSkipSize = lLocalOffset;
            break;
        }
    }
    mValidSeekPosition = true;
    return 0; // success.
}

int ChunkFile::read(QByteArray &pChunk, qint64 pReadSize)
{
    if (mOffset >= size()) {
        return KIO::ERR_NO_CONTENT;
    }
    if (!mValidSeekPosition) {
        return KIO::ERR_CANNOT_READ;
    }

    TreePosition *lCurrentPos = mPositionStack.last();
    if (mCurrentBlob != nullptr && lCurrentPos->mSkipSize == 0) {
        // skipsize has been reset, this means current blob has been exhausted. Free it
        // now as we're about to fetch a new one.
        git_blob_free(mCurrentBlob);
        mCurrentBlob = nullptr;
    }

    if (mCurrentBlob == nullptr) {
        const git_tree_entry *lTreeEntry = git_tree_entry_byindex(lCurrentPos->mTree, lCurrentPos->mIndex);
        if (0 != git_blob_lookup(&mCurrentBlob, mRepository, git_tree_entry_id(lTreeEntry))) {
            return KIO::ERR_CANNOT_READ;
        }
    }

    auto lTotalSize = static_cast<quint64>(git_blob_rawsize(mCurrentBlob));
    if (lTotalSize < lCurrentPos->mSkipSize) { // this must mean a corrupt bup tree somehow
        return KIO::ERR_CANNOT_READ;
    }
    quint64 lAvailableSize = lTotalSize - lCurrentPos->mSkipSize;
    quint64 lReadSize = lAvailableSize;
    if (pReadSize > 0 && static_cast<quint64>(pReadSize) < lAvailableSize) {
        lReadSize = static_cast<quint64>(pReadSize);
    }
    pChunk = QByteArray::fromRawData(static_cast<const char *>(git_blob_rawcontent(mCurrentBlob)) + lCurrentPos->mSkipSize, static_cast<int>(lReadSize));
    mOffset += lReadSize;
    lCurrentPos->mSkipSize += lReadSize;

    // check if it's time to find next blob.
    if (lCurrentPos->mSkipSize == lTotalSize) {
        lCurrentPos->mSkipSize = 0;
        lCurrentPos->mIndex++;
        while (true) {
            if (lCurrentPos->mIndex < git_tree_entrycount(lCurrentPos->mTree)) {
                const git_tree_entry *lTreeEntry = git_tree_entry_byindex(lCurrentPos->mTree, lCurrentPos->mIndex);
                if (S_ISDIR(git_tree_entry_filemode(lTreeEntry))) {
                    git_tree *lTree;
                    if (0 != git_tree_lookup(&lTree, mRepository, git_tree_entry_id(lTreeEntry))) {
                        return KIO::ERR_CANNOT_READ;
                    }
                    lCurrentPos = new TreePosition(lTree); // will have index and skipsize initialized to zero.
                    mPositionStack.append(lCurrentPos);
                } else {
                    // it's a blob
                    break;
                }
            } else {
                delete mPositionStack.takeLast();
                if (mPositionStack.isEmpty()) {
                    Q_ASSERT(mOffset == size());
                    break;
                }
                lCurrentPos = mPositionStack.last();
                lCurrentPos->mIndex++;
            }
        }
    }
    return 0; // success.
}

quint64 ChunkFile::calculateSize()
{
    if (mSize >= 0) {
        return static_cast<quint64>(mSize);
    }
    return calculateChunkFileSize(&mOid, mRepository);
}

ChunkFile::TreePosition::TreePosition(git_tree *pTree)
{
    mTree = pTree;
    mIndex = 0;
    mSkipSize = 0;
}

ChunkFile::TreePosition::~TreePosition()
{
    git_tree_free(mTree);
}

ArchivedDirectory::ArchivedDirectory(Node *pParent, const git_oid *pOid, const QString &pName, qint64 pMode)
    : Directory(pParent, pName, pMode)
    , mOid(*pOid)
    , mTree(nullptr)
    , mMetadataStream(nullptr)
{
    if (0 != git_tree_lookup(&mTree, mRepository, &mOid)) {
        return;
    }
    const git_tree_entry *lTreeEntry = git_tree_entry_byname(mTree, ".bupm");
    if (lTreeEntry != nullptr && 0 == git_blob_lookup(&mMetadataBlob, mRepository, git_tree_entry_id(lTreeEntry))) {
        mMetadataStream = new VintStream(git_blob_rawcontent(mMetadataBlob), static_cast<int>(git_blob_rawsize(mMetadataBlob)), this);
        readMetadata(*mMetadataStream); // the first entry is metadata for the directory itself
    }
}

void ArchivedDirectory::generateSubNodes()
{
    if (mTree == nullptr) {
        return;
    }
    ulong lEntryCount = git_tree_entrycount(mTree);
    for (uint i = 0; i < lEntryCount; ++i) {
        uint lMode;
        const git_oid *lOid;
        QString lName;
        bool lChunked;
        const git_tree_entry *lTreeEntry = git_tree_entry_byindex(mTree, i);
        getEntryAttributes(lTreeEntry, lMode, lChunked, lOid, lName);
        if (lName == QStringLiteral(".bupm")) {
            continue;
        }

        Node *lSubNode = nullptr;
        if (S_ISDIR(lMode)) {
            lSubNode = new ArchivedDirectory(this, lOid, lName, lMode);
        } else if (S_ISLNK(lMode)) {
            lSubNode = new Symlink(this, lOid, lName, lMode);
        } else if (lChunked) {
            lSubNode = new ChunkFile(this, lOid, lName, lMode);
        } else {
            lSubNode = new BlobFile(this, lOid, lName, lMode);
        }
        mSubNodes->insert(lName, lSubNode);
        if (!S_ISDIR(lMode) && mMetadataStream != nullptr) {
            lSubNode->readMetadata(*mMetadataStream);
        }
    }
    if (mMetadataStream != nullptr) {
        delete mMetadataStream;
        mMetadataStream = nullptr;
        git_blob_free(mMetadataBlob);
        mMetadataBlob = nullptr;
    }
    git_tree_free(mTree);
    mTree = nullptr;
}

Branch::Branch(Node *pParent, const char *pName)
    : Directory(pParent, QString::fromLocal8Bit(pName).remove(0, 11), DEFAULT_MODE_DIRECTORY)
    , mRefName(pName)
{
    QByteArray lPath = parent()->objectName().toLocal8Bit();
    lPath.append(mRefName);
    struct stat lStat;
    if (0 == stat(lPath, &lStat)) {
        mAtime = lStat.st_atime;
        mMtime = lStat.st_mtime;
    }
}

void Branch::reload()
{
    if (mSubNodes == nullptr) {
        mSubNodes = new NodeMap();
    }
    // potentially changed content in a branch, generateSubNodes is written so
    // that it can be called repeatedly.
    generateSubNodes();
}

void Branch::generateSubNodes()
{
    if (0 != git_revwalk_push_ref(mRevisionWalker, mRefName)) {
        return;
    }
    git_oid lOid;
    while (0 == git_revwalk_next(&lOid, mRevisionWalker)) {
        git_commit *lCommit;
        if (0 != git_commit_lookup(&lCommit, mRepository, &lOid)) {
            continue;
        }
        QString lCommitTimeLocal = vfsTimeToString(git_commit_time(lCommit));
        if (!mSubNodes->contains(lCommitTimeLocal)) {
            Directory *lDirectory = new ArchivedDirectory(this, git_commit_tree_id(lCommit), lCommitTimeLocal, DEFAULT_MODE_DIRECTORY);
            lDirectory->mMtime = git_commit_time(lCommit);
            mSubNodes->insert(lCommitTimeLocal, lDirectory);
        }
        git_commit_free(lCommit);
    }
}

Repository::Repository(QObject *pParent, const QString &pRepositoryPath)
    : Directory(pParent, pRepositoryPath, DEFAULT_MODE_DIRECTORY)
{
    if (!objectName().endsWith(QLatin1Char('/'))) {
        setObjectName(objectName() + QLatin1Char('/'));
    }
    if (0 != git_repository_open(&mRepository, pRepositoryPath.toLocal8Bit())) {
        qCWarning(KUPKIO) << "could not open repository " << pRepositoryPath;
        mRepository = nullptr;
        return;
    }
    git_strarray lBranchNames;
    git_reference_list(&lBranchNames, mRepository);
    for (uint i = 0; i < lBranchNames.count; ++i) {
        QString lRefName = QString::fromLocal8Bit(lBranchNames.strings[i]);
        if (lRefName.startsWith(QStringLiteral("refs/heads/"))) {
            QString lPath = objectName();
            lPath.append(lRefName);
            struct stat lStat;
            stat(lPath.toLocal8Bit(), &lStat);
            if (lStat.st_atime > mAtime) {
                mAtime = lStat.st_atime;
            }
            if (lStat.st_mtime > mMtime) {
                mMtime = lStat.st_mtime;
            }
        }
    }
    git_strarray_free(&lBranchNames);

    if (0 != git_revwalk_new(&mRevisionWalker, mRepository)) {
        qCWarning(KUPKIO) << "could not create a revision walker in repository " << pRepositoryPath;
        mRevisionWalker = nullptr;
        return;
    }
}

Repository::~Repository()
{
    if (mRepository != nullptr) {
        git_repository_free(mRepository);
    }
    if (mRevisionWalker != nullptr) {
        git_revwalk_free(mRevisionWalker);
    }
}

void Repository::generateSubNodes()
{
    git_strarray lBranchNames;
    git_reference_list(&lBranchNames, mRepository);
    for (uint i = 0; i < lBranchNames.count; ++i) {
        auto lRefName = QString::fromLocal8Bit(lBranchNames.strings[i]);
        if (lRefName.startsWith(QStringLiteral("refs/heads/"))) {
            auto lBranch = new Branch(this, lBranchNames.strings[i]);
            mSubNodes->insert(lBranch->objectName(), lBranch);
        }
    }
    git_strarray_free(&lBranchNames);
}
