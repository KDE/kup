// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "mergedvfs.h"
#include "kupdaemon.h"
#include "kupfiledigger_debug.h"
#include "vfshelpers.h"

#include <KLocalizedString>
#include <KMessageBox>
#include <kwidgetsaddons_version.h>

#include <QDBusInterface>
#include <QDir>
#include <QGuiApplication>

#include <git2/branch.h>
#include <utility>

using NameMap = QMap<QString, MergedNode *>;
using NameMapIterator = QMapIterator<QString, MergedNode *>;

git_repository *MergedNode::mRepository = nullptr;

bool mergedNodeLessThan(const MergedNode *a, const MergedNode *b)
{
    if (a->isDirectory() != b->isDirectory()) {
        return a->isDirectory();
    }
    return a->objectName() < b->objectName();
}

bool versionGreaterThan(const VersionData *a, const VersionData *b)
{
    return a->mModifiedDate > b->mModifiedDate;
}

MergedNode::MergedNode(QObject *pParent, const QString &pName, uint pMode)
    : QObject(pParent)
{
    mSubNodes = nullptr;
    setObjectName(pName);
    mMode = pMode;
}

void MergedNode::getBupUrl(int pVersionIndex, QUrl *pComplete, QString *pRepoPath, QString *pBranchName, qint64 *pCommitTime, QString *pPathInRepo) const
{
    QList<const MergedNode *> lStack;
    const MergedNode *lNode = this;
    while (lNode != nullptr) {
        lStack.append(lNode);
        lNode = qobject_cast<const MergedNode *>(lNode->parent());
    }
    const auto lRepo = qobject_cast<const MergedRepository *>(lStack.takeLast());
    if (pComplete) {
        pComplete->setUrl("bup://" + lRepo->objectName() + lRepo->mBranchName + '/'
                          + vfsTimeToString(static_cast<git_time_t>(mVersionList.at(pVersionIndex)->mCommitTime)));
    }
    if (pRepoPath) {
        *pRepoPath = lRepo->objectName();
    }
    if (pBranchName) {
        *pBranchName = lRepo->mBranchName;
    }
    if (pCommitTime) {
        *pCommitTime = mVersionList.at(pVersionIndex)->mCommitTime;
    }
    if (pPathInRepo) {
        pPathInRepo->clear();
    }
    while (!lStack.isEmpty()) {
        QString lPathComponent = lStack.takeLast()->objectName();
        if (pComplete) {
            pComplete->setPath(pComplete->path() + '/' + lPathComponent);
        }
        if (pPathInRepo) {
            pPathInRepo->append(QLatin1Char('/'));
            pPathInRepo->append(lPathComponent);
        }
    }
}

MergedNodeList &MergedNode::subNodes()
{
    if (mSubNodes == nullptr) {
        mSubNodes = new MergedNodeList();
        if (S_ISDIR(mMode)) {
            QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
            generateSubNodes();
            QGuiApplication::restoreOverrideCursor();
        }
    }
    return *mSubNodes;
}

void MergedNode::askForIntegrityCheck()
{
#if KWIDGETSADDONS_VERSION >= QT_VERSION_CHECK(5, 101, 0)
    int lAnswer = KMessageBox::questionTwoActions(nullptr,
                                                  xi18nc("@info messagebox",
                                                         "Could not read this backup archive. Perhaps some files "
                                                         "have become corrupted. Do you want to run an integrity "
                                                         "check to test this?"),
                                                  QString(),
                                                  KStandardGuiItem::ok(),
                                                  KStandardGuiItem::cancel());
    if (lAnswer == KMessageBox::PrimaryAction) {
#else

    int lAnswer = KMessageBox::questionYesNo(nullptr,
                                             xi18nc("@info messagebox",
                                                    "Could not read this backup archive. Perhaps some files "
                                                    "have become corrupted. Do you want to run an integrity "
                                                    "check to test this?"));
    if (lAnswer == KMessageBox::Yes) {
#endif
        QDBusInterface lInterface(KUP_DBUS_SERVICE_NAME, KUP_DBUS_OBJECT_PATH);
        if (lInterface.isValid()) {
            lInterface.call(QStringLiteral("runIntegrityCheck"), QDir::cleanPath(QString::fromLocal8Bit(git_repository_path(mRepository))));
        }
    }
}

void MergedNode::generateSubNodes()
{
    NameMap lSubNodeMap;
    foreach (VersionData *lCurrentVersion, mVersionList) {
        git_tree *lTree;
        if (0 != git_tree_lookup(&lTree, mRepository, &lCurrentVersion->mOid)) {
            askForIntegrityCheck();
            continue; // try to be fault tolerant by not aborting...
        }
        git_blob *lMetadataBlob = nullptr;
        VintStream *lMetadataStream = nullptr;
        const git_tree_entry *lMetaDataTreeEntry = git_tree_entry_byname(lTree, ".bupm");
        if (lMetaDataTreeEntry != nullptr && 0 == git_blob_lookup(&lMetadataBlob, mRepository, git_tree_entry_id(lMetaDataTreeEntry))) {
            lMetadataStream = new VintStream(git_blob_rawcontent(lMetadataBlob), static_cast<int>(git_blob_rawsize(lMetadataBlob)), this);
            Metadata lMetadata;
            readMetadata(*lMetadataStream, lMetadata); // the first entry is metadata for the directory itself, discard it.
        }

        ulong lEntryCount = git_tree_entrycount(lTree);
        for (uint i = 0; i < lEntryCount; ++i) {
            uint lMode;
            const git_oid *lOid;
            QString lName;
            bool lChunked;
            const git_tree_entry *lTreeEntry = git_tree_entry_byindex(lTree, i);
            getEntryAttributes(lTreeEntry, lMode, lChunked, lOid, lName);
            if (lName == QStringLiteral(".bupm")) {
                continue;
            }

            MergedNode *lSubNode = lSubNodeMap.value(lName, nullptr);
            if (lSubNode == nullptr) {
                lSubNode = new MergedNode(this, lName, lMode);
                lSubNodeMap.insert(lName, lSubNode);
                mSubNodes->append(lSubNode);
            } else if ((S_IFMT & lMode) != (S_IFMT & lSubNode->mMode)) {
                if (S_ISDIR(lMode)) {
                    lName.append(xi18nc("added after folder name in some cases", " (folder)"));
                } else if (S_ISLNK(lMode)) {
                    lName.append(xi18nc("added after file name in some cases", " (symlink)"));
                } else {
                    lName.append(xi18nc("added after file name in some cases", " (file)"));
                }
                lSubNode = lSubNodeMap.value(lName, nullptr);
                if (lSubNode == nullptr) {
                    lSubNode = new MergedNode(this, lName, lMode);
                    lSubNodeMap.insert(lName, lSubNode);
                    mSubNodes->append(lSubNode);
                }
            }
            bool lAlreadySeen = false;
            foreach (VersionData *lVersion, lSubNode->mVersionList) {
                if (lVersion->mOid == *lOid) {
                    lAlreadySeen = true;
                    break;
                }
            }
            if (S_ISDIR(lMode)) {
                if (!lAlreadySeen) {
                    lSubNode->mVersionList.append(new VersionData(lOid, lCurrentVersion->mCommitTime, lCurrentVersion->mModifiedDate, 0));
                }
            } else {
                qint64 lModifiedDate = lCurrentVersion->mModifiedDate;
                qint64 lSize = -1;
                Metadata lMetadata;
                if (lMetadataStream != nullptr && 0 == readMetadata(*lMetadataStream, lMetadata)) {
                    lModifiedDate = lMetadata.mMtime;
                    lSize = lMetadata.mSize;
                }
                if (!lAlreadySeen) {
                    VersionData *lVersionData;
                    if (lSize >= 0) {
                        lVersionData = new VersionData(lOid, lCurrentVersion->mCommitTime, lModifiedDate, static_cast<quint64>(lSize));
                    } else {
                        lVersionData = new VersionData(lChunked, lOid, lCurrentVersion->mCommitTime, lModifiedDate);
                    }
                    lSubNode->mVersionList.append(lVersionData);
                }
            }
        }
        if (lMetadataStream != nullptr) {
            delete lMetadataStream;
            git_blob_free(lMetadataBlob);
        }
        git_tree_free(lTree);
    }
    std::sort(mSubNodes->begin(), mSubNodes->end(), mergedNodeLessThan);
    foreach (MergedNode *lNode, *mSubNodes) {
        std::sort(lNode->mVersionList.begin(), lNode->mVersionList.end(), versionGreaterThan);
    }
}

MergedRepository::MergedRepository(QObject *pParent, const QString &pRepositoryPath, QString pBranchName)
    : MergedNode(pParent, pRepositoryPath, DEFAULT_MODE_DIRECTORY)
    , mBranchName(std::move(pBranchName))
{
    if (!objectName().endsWith(QLatin1Char('/'))) {
        setObjectName(objectName() + QLatin1Char('/'));
    }
}

MergedRepository::~MergedRepository()
{
    if (mRepository != nullptr) {
        git_repository_free(mRepository);
    }
}

bool MergedRepository::open()
{
    if (0 != git_repository_open(&mRepository, objectName().toLocal8Bit())) {
        qCWarning(KUPFILEDIGGER) << "could not open repository " << objectName();
        mRepository = nullptr;
        return false;
    }
    return true;
}

bool MergedRepository::readBranch()
{
    if (mRepository == nullptr) {
        return false;
    }
    git_revwalk *lRevisionWalker;
    if (0 != git_revwalk_new(&lRevisionWalker, mRepository)) {
        qCWarning(KUPFILEDIGGER) << "could not create a revision walker in repository " << objectName();
        return false;
    }

    QString lCompleteBranchName = QStringLiteral("refs/heads/");
    lCompleteBranchName.append(mBranchName);
    if (0 != git_revwalk_push_ref(lRevisionWalker, lCompleteBranchName.toLocal8Bit())) {
        qCWarning(KUPFILEDIGGER) << "Unable to read branch " << mBranchName << " in repository " << objectName();
        git_revwalk_free(lRevisionWalker);
        return false;
    }
    bool lEmptyList = true;
    git_oid lOid;
    while (0 == git_revwalk_next(&lOid, lRevisionWalker)) {
        git_commit *lCommit;
        if (0 != git_commit_lookup(&lCommit, mRepository, &lOid)) {
            continue;
        }
        git_time_t lTime = git_commit_time(lCommit);
        mVersionList.append(new VersionData(git_commit_tree_id(lCommit), lTime, lTime, 0));
        lEmptyList = false;
        git_commit_free(lCommit);
    }
    git_revwalk_free(lRevisionWalker);
    return !lEmptyList;
}

bool MergedRepository::permissionsOk()
{
    if (mRepository == nullptr) {
        return false;
    }
    QDir lRepoDir(objectName());
    if (!lRepoDir.exists()) {
        return false;
    }
    QList<QDir> lDirectories;
    lDirectories << lRepoDir;
    while (!lDirectories.isEmpty()) {
        QDir lDir = lDirectories.takeFirst();
        foreach (QFileInfo lFileInfo, lDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
            if (!lFileInfo.isReadable()) {
                return false;
            }
            if (lFileInfo.isDir()) {
                lDirectories << QDir(lFileInfo.absoluteFilePath());
            }
        }
    }
    return true;
}

uint qHash(git_oid pOid)
{
    return qHash(QByteArray::fromRawData(reinterpret_cast<const char *>(pOid.id), GIT_OID_RAWSZ));
}

bool operator==(const git_oid &pOidA, const git_oid &pOidB)
{
    QByteArray a = QByteArray::fromRawData(reinterpret_cast<const char *>(pOidA.id), GIT_OID_RAWSZ);
    QByteArray b = QByteArray::fromRawData(reinterpret_cast<const char *>(pOidB.id), GIT_OID_RAWSZ);
    return a == b;
}

quint64 VersionData::size()
{
    if (mSizeIsValid) {
        return mSize;
    }
    if (mChunkedFile) {
        mSize = calculateChunkFileSize(&mOid, MergedNode::mRepository);
    } else {
        git_blob *lBlob;
        if (0 == git_blob_lookup(&lBlob, MergedNode::mRepository, &mOid)) {
            mSize = static_cast<quint64>(git_blob_rawsize(lBlob));
            git_blob_free(lBlob);
        } else {
            mSize = 0;
        }
    }
    mSizeIsValid = true;
    return mSize;
}
