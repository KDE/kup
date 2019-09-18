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

#ifndef VFSHELPERS_H
#define VFSHELPERS_H

#include <QObject>
#include <QString>
class QBuffer;

#include <git2.h>

#define DEFAULT_MODE_DIRECTORY 0040755
#define DEFAULT_MODE_FILE 0100644

class VintStream: public QObject {
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
	Metadata() {}
	Metadata(quint64 pMode);
	qint64 mMode;
	qint64 mUid;
	qint64 mGid;
	qint64 mAtime;
	qint64 mMtime;
	QString mSymlinkTarget;

	static quint64 mDefaultUid;
	static quint64 mDefaultGid;
	static bool mDefaultsResolved;
};

int readMetadata(VintStream &pMetadataStream, Metadata &pMetadata);
quint64 calculateChunkFileSize(const git_oid *pOid, git_repository *pRepository);
bool offsetFromName(const git_tree_entry *pEntry, quint64 &pUint);
void getEntryAttributes(const git_tree_entry *pTreeEntry, uint &pMode, bool &pChunked, const git_oid *&pOid, QString &pName);
QString vfsTimeToString(git_time_t pTime);

#endif // VFSHELPERS_H
