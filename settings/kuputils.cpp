// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "kuputils.h"

#include <QDir>

void ensureTrailingSlash(QString &pPath)
{
    if (!pPath.endsWith(QDir::separator())) {
        pPath.append(QDir::separator());
    }
}

void ensureNoTrailingSlash(QString &pPath)
{
    while (pPath.endsWith(QDir::separator())) {
        pPath.chop(1);
    }
}

QString lastPartOfPath(const QString &pPath)
{
    return pPath.section(QDir::separator(), -1, -1, QString::SectionSkipEmpty);
}
