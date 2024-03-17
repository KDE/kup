// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "vfshelpers.h"

#include <QBuffer>
#include <QByteArray>
#include <QDateTime>

#include <sys/stat.h>
#include <unistd.h>

static const int cRecordEnd = 0;
// static const int cRecordPath = 1;
static const int cRecordCommonV1 = 2; // times, user, group, type, perms, etc. (legacy version 1)
static const int cRecordSymlinkTarget = 3;
// static const int cRecordPosix1eAcl = 4; // getfacl(1), setfacl(1), etc.
// static const int cRecordNfsV4Acl = 5; // intended to supplant posix1e acls?
// static const int cRecordLinuxAttr = 6; // lsattr(1) chattr(1)
// static const int cRecordLinuxXattr = 7; // getfattr(1) setfattr(1)
// static const int cRecordHardlinkTarget = 8;
static const int cRecordCommonV2 = 9; // times, user, group, type, perms, etc.
static const int cRecordCommonV3 = 10; // times, user, group, type, perms, etc.

VintStream::VintStream(const void *pData, int pSize, QObject *pParent)
    : QObject(pParent)
    , mByteArray(QByteArray::fromRawData(static_cast<const char *>(pData), pSize))
    , mBuffer(new QBuffer(&mByteArray, this))
{
    mBuffer->open(QIODevice::ReadOnly);
}

VintStream &VintStream::operator>>(qint64 &pInt)
{
    char c;
    if (!mBuffer->getChar(&c)) {
        throw 1;
    }
    int lOffset = 6;
    bool lNegative = (c & 0x40);
    pInt = c & 0x3F;
    while (c & 0x80) {
        if (!mBuffer->getChar(&c)) {
            throw 1;
        }
        pInt |= (c & 0x7F) << lOffset;
        lOffset += 7;
    }
    if (lNegative) {
        pInt = -pInt;
    }
    return *this;
}

VintStream &VintStream::operator>>(quint64 &pUint)
{
    char c;
    int lOffset = 0;
    pUint = 0;
    do {
        if (!mBuffer->getChar(&c)) {
            throw 1;
        }
        pUint |= static_cast<quint64>((c & 0x7F) << lOffset);
        lOffset += 7;
    } while (c & 0x80);
    return *this;
}

VintStream &VintStream::operator>>(QString &pString)
{
    QByteArray lBytes;
    *this >> lBytes;
    pString = QString::fromUtf8(lBytes);
    return *this;
}

VintStream &VintStream::operator>>(QByteArray &pByteArray)
{
    quint64 lByteCount;
    *this >> lByteCount;
    pByteArray.resize(static_cast<int>(lByteCount));
    if (mBuffer->read(pByteArray.data(), pByteArray.length()) < pByteArray.length()) {
        throw 1;
    }
    return *this;
}

qint64 Metadata::mDefaultUid;
qint64 Metadata::mDefaultGid;
bool Metadata::mDefaultsResolved = false;

Metadata::Metadata(qint64 pMode)
{
    mMode = pMode;
    mAtime = 0;
    mMtime = 0;
    if (!mDefaultsResolved) {
        mDefaultUid = getuid();
        mDefaultGid = getgid();
        mDefaultsResolved = true;
    }
    mUid = mDefaultUid;
    mGid = mDefaultGid;
    mSize = -1;
}

int readMetadata(VintStream &pMetadataStream, Metadata &pMetadata)
{
    try {
        quint64 lTag;
        do {
            pMetadataStream >> lTag;
            switch (lTag) {
            case cRecordCommonV1: {
                qint64 lNotUsedInt;
                quint64 lNotUsedUint, lTempUint;
                QString lNotUsedString;
                pMetadataStream >> lNotUsedUint >> lTempUint;
                pMetadata.mMode = static_cast<qint64>(lTempUint);
                pMetadataStream >> lTempUint >> lNotUsedString; // user name
                pMetadata.mUid = static_cast<qint64>(lTempUint);
                pMetadataStream >> lTempUint >> lNotUsedString; // group name
                pMetadata.mGid = static_cast<qint64>(lTempUint);
                pMetadataStream >> lNotUsedUint; // device number
                pMetadataStream >> pMetadata.mAtime >> lNotUsedUint; // nanoseconds
                pMetadataStream >> pMetadata.mMtime >> lNotUsedUint; // nanoseconds
                pMetadataStream >> lNotUsedInt >> lNotUsedUint; // status change time
                break;
            }
            case cRecordCommonV2: {
                qint64 lNotUsedInt;
                quint64 lNotUsedUint;
                QString lNotUsedString;
                pMetadataStream >> lNotUsedUint >> pMetadata.mMode;
                pMetadataStream >> pMetadata.mUid >> lNotUsedString; // user name
                pMetadataStream >> pMetadata.mGid >> lNotUsedString; // group name
                pMetadataStream >> lNotUsedInt; // device number
                pMetadataStream >> pMetadata.mAtime >> lNotUsedUint; // nanoseconds
                pMetadataStream >> pMetadata.mMtime >> lNotUsedUint; // nanoseconds
                pMetadataStream >> lNotUsedInt >> lNotUsedUint; // status change time
                break;
            }
            case cRecordCommonV3: {
                qint64 lNotUsedInt;
                quint64 lNotUsedUint;
                QString lNotUsedString;
                pMetadataStream >> lNotUsedUint >> pMetadata.mMode;
                pMetadataStream >> pMetadata.mUid >> lNotUsedString; // user name
                pMetadataStream >> pMetadata.mGid >> lNotUsedString; // group name
                pMetadataStream >> lNotUsedInt; // device number
                pMetadataStream >> pMetadata.mAtime >> lNotUsedUint; // nanoseconds
                pMetadataStream >> pMetadata.mMtime >> lNotUsedUint; // nanoseconds
                pMetadataStream >> lNotUsedInt >> lNotUsedUint; // status change time
                pMetadataStream >> pMetadata.mSize;
                break;
            }
            case cRecordSymlinkTarget: {
                pMetadataStream >> pMetadata.mSymlinkTarget;
                break;
            }
            default: {
                if (lTag != cRecordEnd) {
                    QByteArray lNotUsed;
                    pMetadataStream >> lNotUsed;
                }
                break;
            }
            }
        } while (lTag != cRecordEnd);
    } catch (int) {
        return 1;
    }
    return 0; // success
}

quint64 calculateChunkFileSize(const git_oid *pOid, git_repository *pRepository)
{
    quint64 lLastChunkOffset = 0;
    quint64 lLastChunkSize = 0;
    uint lMode;
    do {
        git_tree *lTree;
        if (0 != git_tree_lookup(&lTree, pRepository, pOid)) {
            return 0;
        }
        ulong lEntryCount = git_tree_entrycount(lTree);
        const git_tree_entry *lEntry = git_tree_entry_byindex(lTree, lEntryCount - 1);
        quint64 lEntryOffset;
        if (!offsetFromName(lEntry, lEntryOffset)) {
            git_tree_free(lTree);
            return 0;
        }
        lLastChunkOffset += lEntryOffset;
        pOid = git_tree_entry_id(lEntry);
        lMode = git_tree_entry_filemode(lEntry);
        git_tree_free(lTree);
    } while (S_ISDIR(lMode));

    git_blob *lBlob;
    if (0 != git_blob_lookup(&lBlob, pRepository, pOid)) {
        return 0;
    }
    lLastChunkSize = static_cast<quint64>(git_blob_rawsize(lBlob));
    git_blob_free(lBlob);
    return lLastChunkOffset + lLastChunkSize;
}

bool offsetFromName(const git_tree_entry *pEntry, quint64 &pUint)
{
    bool lParsedOk;
    pUint = QString::fromUtf8(git_tree_entry_name(pEntry)).toULongLong(&lParsedOk, 16);
    return lParsedOk;
}

void getEntryAttributes(const git_tree_entry *pTreeEntry, uint &pMode, bool &pChunked, const git_oid *&pOid, QString &pName)
{
    pMode = git_tree_entry_filemode(pTreeEntry);
    pOid = git_tree_entry_id(pTreeEntry);
    pName = QString::fromUtf8(git_tree_entry_name(pTreeEntry));
    pChunked = false;
    if (pName.endsWith(QStringLiteral(".bupl"))) {
        pName.chop(5);
    } else if (pName.endsWith(QStringLiteral(".bup"))) {
        pName.chop(4);
        pMode = DEFAULT_MODE_FILE;
        pChunked = true;
    }
}

QString vfsTimeToString(git_time_t pTime)
{
    QDateTime lDateTime;
    lDateTime.setSecsSinceEpoch(pTime);
    return lDateTime.toLocalTime().toString(QStringLiteral("yyyy-MM-dd hh:mm"));
}
