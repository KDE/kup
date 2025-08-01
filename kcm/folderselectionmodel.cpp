// SPDX-FileCopyrightText: 2003 Scott Wheeler <wheeler@kde.org>
// SPDX-FileCopyrightText: 2004 Max Howell <max.howell@methylblue.com>
// SPDX-FileCopyrightText: 2004 Mark Kretschmann <markey@web.de>
// SPDX-FileCopyrightText: 2008 Seb Ruiz <ruiz@kde.org>
// SPDX-FileCopyrightText: 2008 Sebastian Trueg <trueg@kde.org>
// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-2.0-only

#include "folderselectionmodel.h"
#include "kuputils.h"

#include <QBrush>
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QPalette>

#include <KLocalizedString>

namespace
{

bool setContainsSubdir(const QSet<QString> &pSet, const QString &pParentDir)
{
    // we need the trailing slash to be able to use the startsWith() function to check for parent dirs.
    QString lPathWithSlash = pParentDir;
    ensureTrailingSlash(lPathWithSlash);
    return std::any_of(pSet.cbegin(), pSet.cend(), [&](auto pTestedPath) {
        return pTestedPath.startsWith(lPathWithSlash);
    });
}

}

FolderSelectionModel::FolderSelectionModel(bool pHiddenFoldersVisible, QObject *pParent)
    : QFileSystemModel(pParent)
{
    setHiddenFoldersVisible(pHiddenFoldersVisible);
}

Qt::ItemFlags FolderSelectionModel::flags(const QModelIndex &pIndex) const
{
    Qt::ItemFlags lFlags = QFileSystemModel::flags(pIndex);
    lFlags |= Qt::ItemIsUserCheckable;
    return lFlags;
}

QVariant FolderSelectionModel::data(const QModelIndex &pIndex, int pRole) const
{
    if (!pIndex.isValid() || pIndex.column() != 0) {
        return QFileSystemModel::data(pIndex, pRole);
    }
    const QString lPath = filePath(pIndex);
    const InclusionState lState = inclusionState(lPath);
    switch (pRole) {
    case Qt::CheckStateRole: {
        switch (lState) {
        case StateIncluded:
        case StateIncludeInherited:
            if (setContainsSubdir(mExcludedPaths, lPath)) {
                return Qt::PartiallyChecked;
            }
            return Qt::Checked;
        default:
            return Qt::Unchecked;
        }
    }
    case IncludeStateRole:
        return inclusionState(pIndex);
    case Qt::ForegroundRole: {
        switch (lState) {
        case StateIncluded:
        case StateIncludeInherited:
            return QVariant::fromValue(QPalette().brush(QPalette::Active, QPalette::Text));
        default:
            if (setContainsSubdir(mIncludedPaths, lPath)) {
                return QVariant::fromValue(QPalette().brush(QPalette::Active, QPalette::Text));
            }
            return QVariant::fromValue(QPalette().brush(QPalette::Disabled, QPalette::Text));
        }
    }
    case Qt::ToolTipRole: {
        switch (lState) {
        case StateIncluded:
        case StateIncludeInherited:
            if (setContainsSubdir(mExcludedPaths, lPath)) {
                return xi18nc("@info:tooltip %1 is the path of the folder in a listview",
                              "<filename>%1</filename><nl/>will be included in the backup, except "
                              "for unchecked subfolders",
                              filePath(pIndex));
            }
            return xi18nc("@info:tooltip %1 is the path of the folder in a listview",
                          "<filename>%1</filename><nl/>will be included in the backup",
                          filePath(pIndex));
        default:
            if (setContainsSubdir(mIncludedPaths, lPath)) {
                return xi18nc("@info:tooltip %1 is the path of the folder in a listview",
                              "<filename>%1</filename><nl/> will <emphasis>not</emphasis> be included "
                              "in the backup but contains folders that will",
                              filePath(pIndex));
            }
            return xi18nc("@info:tooltip %1 is the path of the folder in a listview",
                          "<filename>%1</filename><nl/> will <emphasis>not</emphasis> be included "
                          "in the backup",
                          filePath(pIndex));
        }
    }
    case Qt::DecorationRole:
        if (lPath == QDir::homePath()) {
            return QIcon::fromTheme(QStringLiteral("user-home"));
        }
        break;
    }

    return QFileSystemModel::data(pIndex, pRole);
}

bool FolderSelectionModel::setData(const QModelIndex &pIndex, const QVariant &pValue, int pRole)
{
    if (!pIndex.isValid() || pIndex.column() != 0 || pRole != Qt::CheckStateRole) {
        return QFileSystemModel::setData(pIndex, pValue, pRole);
    }

    // here we ignore the check value, we treat it as a toggle
    // This is due to our using the Qt checking system in a virtual way
    const QString lPath = filePath(pIndex);
    const InclusionState lState = inclusionState(lPath);
    switch (lState) {
    case StateIncluded:
    case StateIncludeInherited:
        excludePath(lPath);
        break;
    default:
        includePath(lPath);
    }
    QModelIndex lRecurseIndex = pIndex;
    while (lRecurseIndex.isValid()) {
        emit dataChanged(lRecurseIndex, lRecurseIndex);
        lRecurseIndex = lRecurseIndex.parent();
    }
    return true;
}

void FolderSelectionModel::includePath(const QString &pPath)
{
    const InclusionState lState = inclusionState(pPath);
    if (lState == StateIncluded) {
        return;
    }
    removeSubDirs(pPath);
    if (lState == StateNone || lState == StateExcludeInherited) {
        mIncludedPaths.insert(pPath);
        emit includedPathAdded(pPath);
    }
    emit dataChanged(index(pPath), findLastLeaf(index(pPath)));
}

void FolderSelectionModel::excludePath(const QString &pPath)
{
    const InclusionState lState = inclusionState(pPath);
    if (lState == StateExcluded) {
        return;
    }
    removeSubDirs(pPath);
    if (lState == StateIncludeInherited) {
        mExcludedPaths.insert(pPath);
        emit excludedPathAdded(pPath);
    }
    emit dataChanged(index(pPath), findLastLeaf(index(pPath)));
}

void FolderSelectionModel::setIncludedPaths(const QSet<QString> &pIncludedPaths)
{
    QSet<QString> lRemoved = mIncludedPaths - pIncludedPaths;
    QSet<QString> lAdded = pIncludedPaths - mIncludedPaths;
    if (lRemoved.count() + lAdded.count() == 0)
        return;

    beginResetModel();
    mIncludedPaths = pIncludedPaths;
    foreach (const QString &lRemovedPath, lRemoved) {
        emit includedPathRemoved(lRemovedPath);
    }
    foreach (const QString &lAddedPath, lAdded) {
        emit includedPathAdded(lAddedPath);
    }
    endResetModel();
}

void FolderSelectionModel::setExcludedPaths(const QSet<QString> &pExcludedPaths)
{
    QSet<QString> lRemoved = mExcludedPaths - pExcludedPaths;
    QSet<QString> lAdded = pExcludedPaths - mExcludedPaths;
    if (lRemoved.count() + lAdded.count() == 0)
        return;

    beginResetModel();
    mExcludedPaths = pExcludedPaths;
    foreach (const QString &lRemovedPath, lRemoved) {
        emit excludedPathRemoved(lRemovedPath);
    }
    foreach (const QString &lAddedPath, lAdded) {
        emit excludedPathAdded(lAddedPath);
    }
    endResetModel();
}

QSet<QString> FolderSelectionModel::includedPaths() const
{
    return mIncludedPaths;
}

QSet<QString> FolderSelectionModel::excludedPaths() const
{
    return mExcludedPaths;
}

FolderSelectionModel::InclusionState FolderSelectionModel::inclusionState(const QModelIndex &pIndex) const
{
    return inclusionState(filePath(pIndex));
}

FolderSelectionModel::InclusionState FolderSelectionModel::inclusionState(const QString &pPath) const
{
    if (mIncludedPaths.contains(pPath)) {
        return StateIncluded;
    }
    if (mExcludedPaths.contains(pPath)) {
        return StateExcluded;
    }
    QString lParent = pPath.section(QDir::separator(), 0, -2, QString::SectionSkipEmpty | QString::SectionIncludeLeadingSep);
    if (lParent.isEmpty()) {
        return StateNone;
    }
    InclusionState state = inclusionState(lParent);
    if (state == StateNone) {
        return StateNone;
    }
    if (state == StateIncluded || state == StateIncludeInherited) {
        return StateIncludeInherited;
    }
    return StateExcludeInherited;
}

bool FolderSelectionModel::hiddenFoldersVisible() const
{
    return filter() & QDir::Hidden;
}

void FolderSelectionModel::setHiddenFoldersVisible(bool pVisible)
{
    if (pVisible) {
        setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Hidden);
    } else {
        setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);
    }
}

QModelIndex FolderSelectionModel::findLastLeaf(const QModelIndex &pIndex)
{
    QModelIndex lIndex = pIndex;
    forever {
        int lRowCount = rowCount(lIndex);
        if (lRowCount > 0) {
            lIndex = index(lRowCount - 1, 0, lIndex);
        } else {
            return lIndex;
        }
    }
}

void FolderSelectionModel::removeSubDirs(const QString &pPath)
{
    QSet<QString>::iterator it = mExcludedPaths.begin();
    QString lPath = pPath + QStringLiteral("/");
    while (it != mExcludedPaths.end()) {
        if (*it == pPath || it->startsWith(lPath)) {
            QString lPathCopy = *it;
            it = mExcludedPaths.erase(it);
            emit excludedPathRemoved(lPathCopy);
        } else {
            ++it;
        }
    }
    it = mIncludedPaths.begin();
    while (it != mIncludedPaths.end()) {
        if (*it == pPath || it->startsWith(lPath)) {
            QString lPathCopy = *it;
            it = mIncludedPaths.erase(it);
            emit includedPathRemoved(lPathCopy);
        } else {
            ++it;
        }
    }
}
