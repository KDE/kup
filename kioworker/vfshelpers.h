// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef VFSHELPERS_H
#define VFSHELPERS_H

#include <QObject>
#include <QString>
class QBuffer;

#include <git2.h>

#define DEFAULT_MODE_DIRECTORY 0040755
#define DEFAULT_MODE_FILE 0100644

class VintStream : public QObject
{
    Q_OBJECT

public:
    VintStream(const void *pData, int pSize, QObject *pParent);

    VintStream &operator>>(qint64 &pInt);
    VintStream &operator>>(quint64 &pUint);
    VintStream &operator>>(QString &pString);
    VintStream &operator>>(QByteArray &pByteArray);

protected:
    QByteArray mByteArray;
    QBuffer *mBuffer;
};

struct Metadata {
    Metadata()
    {
    }
    explicit Metadata(qint64 pMode);
    qint64 mMode;
    qint64 mUid;
    qint64 mGid;
    qint64 mAtime;
    qint64 mMtime;
    qint64 mSize; // negative if invalid
    QString mSymlinkTarget;

    static qint64 mDefaultUid;
    static qint64 mDefaultGid;
    static bool mDefaultsResolved;
};

int readMetadata(VintStream &pMetadataStream, Metadata &pMetadata);
quint64 calculateChunkFileSize(const git_oid *pOid, git_repository *pRepository);
bool offsetFromName(const git_tree_entry *pEntry, quint64 &pUint);
void getEntryAttributes(const git_tree_entry *pTreeEntry, uint &pMode, bool &pChunked, const git_oid *&pOid, QString &pName);
QString vfsTimeToString(git_time_t pTime);

#endif // VFSHELPERS_H
