// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL

#ifndef KUPUTILS_H
#define KUPUTILS_H

class QString;

void ensureTrailingSlash(QString &pPath);

void ensureNoTrailingSlash(QString &pPath);

QString lastPartOfPath(const QString &pPath);

#endif // KUPUTILS_H
