// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "bupvfs.h"

#include <QCoreApplication>
#include <QFile>
#include <QUrl>
#include <QVarLengthArray>

#include <KIO/WorkerBase>
using namespace KIO;
#include <KLocalizedString>
#include <KProcess>

#include <grp.h>
#include <pwd.h>

// Pseudo plugin class to embed meta data
class KIOPluginForMetaData : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kio.worker.bup" FILE "bup.json")
};

class BupWorker : public WorkerBase
{
public:
    BupWorker(const QByteArray &pPoolSocket, const QByteArray &pAppSocket);
    ~BupWorker() override;
    KIO::WorkerResult close() override;
    KIO::WorkerResult get(const QUrl &pUrl) override;
    KIO::WorkerResult listDir(const QUrl &pUrl) override;
    KIO::WorkerResult open(const QUrl &pUrl, QIODevice::OpenMode pMode) override;
    KIO::WorkerResult read(filesize_t pSize) override;
    KIO::WorkerResult seek(filesize_t pOffset) override;
    KIO::WorkerResult stat(const QUrl &pUrl) override;
    KIO::WorkerResult mimetype(const QUrl &pUrl) override;

private:
    bool checkCorrectRepository(const QUrl &pUrl, QStringList &pPathInRepository);
    QString getUserName(uid_t pUid);
    QString getGroupName(gid_t pGid);
    void createUDSEntry(Node *pNode, KIO::UDSEntry &pUDSEntry, int pDetails);

    QHash<uid_t, QString> mUsercache;
    QHash<gid_t, QString> mGroupcache;
    Repository *mRepository;
    File *mOpenFile;
};

BupWorker::BupWorker(const QByteArray &pPoolSocket, const QByteArray &pAppSocket)
    : WorkerBase("bup", pPoolSocket, pAppSocket)
{
    mRepository = nullptr;
    mOpenFile = nullptr;
    git_libgit2_init();
}

BupWorker::~BupWorker()
{
    delete mRepository;
    git_libgit2_shutdown();
}

KIO::WorkerResult BupWorker::close()
{
    mOpenFile = nullptr;
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult BupWorker::get(const QUrl &pUrl)
{
    QStringList lPathInRepo;
    if (!checkCorrectRepository(pUrl, lPathInRepo)) {
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, i18n("No bup repository found.\n%1", pUrl.toDisplayString()));
    }

    // Assume that a symlink should be followed.
    // Kio will never call get() on a symlink if it actually wants to copy a
    // symlink, it would just create a symlink on the destination kioworker using the
    // target it already got from calling stat() on this one.
    Node *lNode = mRepository->resolve(lPathInRepo, true);
    if (lNode == nullptr) {
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, lPathInRepo.join(QStringLiteral("/")));
    }
    File *lFile = qobject_cast<File *>(lNode);
    if (lFile == nullptr) {
        return KIO::WorkerResult::fail(KIO::ERR_IS_DIRECTORY, lPathInRepo.join(QStringLiteral("/")));
    }

    mimeType(lFile->mMimeType);
    // Emit total size AFTER mimetype
    totalSize(lFile->size());

    // make sure file is at the beginning
    lFile->seek(0);
    KIO::filesize_t lProcessedSize = 0;
    const QString lResumeOffset = metaData(QStringLiteral("resume"));
    if (!lResumeOffset.isEmpty()) {
        bool ok;
        quint64 lOffset = lResumeOffset.toULongLong(&ok);
        if (ok && lOffset < lFile->size()) {
            if (0 == lFile->seek(lOffset)) {
                canResume();
                lProcessedSize = lOffset;
            }
        }
    }

    QByteArray lResultArray;
    int lRetVal;
    while (0 == (lRetVal = lFile->read(lResultArray))) {
        data(lResultArray);
        lProcessedSize += static_cast<quint64>(lResultArray.length());
        processedSize(lProcessedSize);
    }
    if (lRetVal == KIO::ERR_NO_CONTENT) {
        data(QByteArray());
        processedSize(lProcessedSize);
        return KIO::WorkerResult::pass();
    } else {
        return KIO::WorkerResult::fail(lRetVal, lPathInRepo.join(QStringLiteral("/")));
    }
}

KIO::WorkerResult BupWorker::listDir(const QUrl &pUrl)
{
    QStringList lPathInRepo;
    if (!checkCorrectRepository(pUrl, lPathInRepo)) {
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, i18n("No bup repository found.\n%1", pUrl.toDisplayString()));
    }
    Node *lNode = mRepository->resolve(lPathInRepo, true);
    if (lNode == nullptr) {
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, lPathInRepo.join(QStringLiteral("/")));
    }
    auto lDir = qobject_cast<Directory *>(lNode);
    if (lDir == nullptr) {
        return KIO::WorkerResult::fail(KIO::ERR_IS_FILE, lPathInRepo.join(QStringLiteral("/")));
    }

    // give the directory a chance to reload if necessary.
    lDir->reload();

    const QString sDetails = metaData(QStringLiteral("details"));
    const int lDetails = sDetails.isEmpty() ? 2 : sDetails.toInt();

    NodeMapIterator i(lDir->subNodes());
    UDSEntry lEntry;
    while (i.hasNext()) {
        createUDSEntry(i.next().value(), lEntry, lDetails);
        listEntry(lEntry);
    }
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult BupWorker::open(const QUrl &pUrl, QIODevice::OpenMode pMode)
{
    if (pMode & QIODevice::WriteOnly) {
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_OPEN_FOR_WRITING, pUrl.toDisplayString());
    }

    QStringList lPathInRepo;
    if (!checkCorrectRepository(pUrl, lPathInRepo)) {
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, i18n("No bup repository found.\n%1", pUrl.toDisplayString()));
    }

    Node *lNode = mRepository->resolve(lPathInRepo, true);
    if (lNode == nullptr) {
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, lPathInRepo.join(QStringLiteral("/")));
    }

    File *lFile = qobject_cast<File *>(lNode);
    if (lFile == nullptr) {
        return KIO::WorkerResult::fail(KIO::ERR_IS_DIRECTORY, lPathInRepo.join(QStringLiteral("/")));
    }

    if (0 != lFile->seek(0)) {
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_OPEN_FOR_READING, pUrl.toDisplayString());
    }

    mOpenFile = lFile;
    mimeType(lFile->mMimeType);
    totalSize(lFile->size());
    position(0);
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult BupWorker::read(filesize_t pSize)
{
    if (mOpenFile == nullptr) {
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_READ, QString());
    }
    QByteArray lResultArray;
    int lRetVal = 0;
    while (pSize > 0 && 0 == (lRetVal = mOpenFile->read(lResultArray, static_cast<int>(pSize)))) {
        pSize -= static_cast<quint64>(lResultArray.size());
        data(lResultArray);
    }
    if (lRetVal == 0) {
        data(QByteArray());
        return KIO::WorkerResult::pass();
    } else {
        return KIO::WorkerResult::fail(lRetVal, mOpenFile->completePath());
    }
}

KIO::WorkerResult BupWorker::seek(filesize_t pOffset)
{
    if (mOpenFile == nullptr) {
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_SEEK, QString());
    }

    if (0 != mOpenFile->seek(pOffset)) {
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_SEEK, mOpenFile->completePath());
    }
    position(pOffset);
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult BupWorker::stat(const QUrl &pUrl)
{
    QStringList lPathInRepo;
    if (!checkCorrectRepository(pUrl, lPathInRepo)) {
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, i18n("No bup repository found.\n%1", pUrl.toDisplayString()));
    }

    Node *lNode = mRepository->resolve(lPathInRepo);
    if (lNode == nullptr) {
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, lPathInRepo.join(QStringLiteral("/")));
    }

    const QString sDetails = metaData(QStringLiteral("details"));
    const int lDetails = sDetails.isEmpty() ? 2 : sDetails.toInt();

    UDSEntry lUDSEntry;
    createUDSEntry(lNode, lUDSEntry, lDetails);
    statEntry(lUDSEntry);
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult BupWorker::mimetype(const QUrl &pUrl)
{
    QStringList lPathInRepo;
    if (!checkCorrectRepository(pUrl, lPathInRepo)) {
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, i18n("No bup repository found.\n%1", pUrl.toDisplayString()));
    }

    Node *lNode = mRepository->resolve(lPathInRepo);
    if (lNode == nullptr) {
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, lPathInRepo.join(QStringLiteral("/")));
    }

    mimeType(lNode->mMimeType);
    return KIO::WorkerResult::pass();
}

bool BupWorker::checkCorrectRepository(const QUrl &pUrl, QStringList &pPathInRepository)
{
    // make this worker accept most URLs.. even incorrect ones. (no slash (wrong),
    // one slash (correct), two slashes (wrong), three slashes (correct))
    QString lPath;
    if (!pUrl.host().isEmpty()) {
        lPath = QStringLiteral("/") + pUrl.host() + pUrl.adjusted(QUrl::StripTrailingSlash).path() + '/';
    } else {
        lPath = pUrl.adjusted(QUrl::StripTrailingSlash).path() + '/';
        if (!lPath.startsWith(QLatin1Char('/'))) {
            lPath.prepend(QLatin1Char('/'));
        }
    }

    if (mRepository && mRepository->isValid()) {
        if (lPath.startsWith(mRepository->objectName())) {
            lPath.remove(0, mRepository->objectName().length());
            pPathInRepository = lPath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
            return true;
        }
        delete mRepository;
        mRepository = nullptr;
    }

    pPathInRepository = lPath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QString lRepoPath = QStringLiteral("/");
    while (!pPathInRepository.isEmpty()) {
        // make sure the repo path will end with a slash
        lRepoPath += pPathInRepository.takeFirst();
        lRepoPath += QStringLiteral("/");
        if ((QFile::exists(lRepoPath + QStringLiteral("objects")) && QFile::exists(lRepoPath + QStringLiteral("refs")))
            || (QFile::exists(lRepoPath + QStringLiteral(".git/objects")) && QFile::exists(lRepoPath + QStringLiteral(".git/refs")))) {
            mRepository = new Repository(nullptr, lRepoPath);
            return mRepository->isValid();
        }
    }
    return false;
}

QString BupWorker::getUserName(uid_t pUid)
{
    if (!mUsercache.contains(pUid)) {
        struct passwd *lUserInfo = getpwuid(pUid);
        if (lUserInfo) {
            mUsercache.insert(pUid, QString::fromLocal8Bit(lUserInfo->pw_name));
        } else {
            return QString::number(pUid);
        }
    }
    return mUsercache.value(pUid);
}

QString BupWorker::getGroupName(gid_t pGid)
{
    if (!mGroupcache.contains(pGid)) {
        struct group *lGroupInfo = getgrgid(pGid);
        if (lGroupInfo) {
            mGroupcache.insert(pGid, QString::fromLocal8Bit(lGroupInfo->gr_name));
        } else {
            return QString::number(pGid);
        }
    }
    return mGroupcache.value(pGid);
}

void BupWorker::createUDSEntry(Node *pNode, UDSEntry &pUDSEntry, int pDetails)
{
    pUDSEntry.clear();
    pUDSEntry.fastInsert(KIO::UDSEntry::UDS_NAME, pNode->objectName());
    if (!pNode->mSymlinkTarget.isEmpty()) {
        pUDSEntry.fastInsert(KIO::UDSEntry::UDS_LINK_DEST, pNode->mSymlinkTarget);
        if (pDetails > 1) {
            Node *lNode = qobject_cast<Node *>(pNode->parent())->resolve(pNode->mSymlinkTarget, true);
            if (lNode != nullptr) { // follow symlink only if details > 1 and it leads to something
                pNode = lNode;
            }
        }
    }
    pUDSEntry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, pNode->mMode & S_IFMT);
    pUDSEntry.fastInsert(KIO::UDSEntry::UDS_ACCESS, pNode->mMode & 07777);
    if (pDetails > 0) {
        quint64 lSize = 0;
        File *lFile = qobject_cast<File *>(pNode);
        if (lFile != nullptr) {
            lSize = lFile->size();
        }
        pUDSEntry.fastInsert(KIO::UDSEntry::UDS_SIZE, static_cast<qint64>(lSize));
        pUDSEntry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, pNode->mMimeType);
        pUDSEntry.fastInsert(KIO::UDSEntry::UDS_ACCESS_TIME, pNode->mAtime);
        pUDSEntry.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, pNode->mMtime);
        pUDSEntry.fastInsert(KIO::UDSEntry::UDS_USER, getUserName(static_cast<uint>(pNode->mUid)));
        pUDSEntry.fastInsert(KIO::UDSEntry::UDS_GROUP, getGroupName(static_cast<uint>(pNode->mGid)));
    }
}

extern "C" int Q_DECL_EXPORT kdemain(int pArgc, char **pArgv)
{
    QCoreApplication lApp(pArgc, pArgv);
    QCoreApplication::setApplicationName(QStringLiteral("kio_bup"));
    KLocalizedString::setApplicationDomain("kup");

    if (pArgc != 4) {
        fprintf(stderr, "Usage: kio_bup protocol domain-socket1 domain-socket2\n");
        exit(-1);
    }

    BupWorker lWorker(pArgv[2], pArgv[3]);
    lWorker.dispatchLoop();

    return 0;
}

#include "bupworker.moc"
