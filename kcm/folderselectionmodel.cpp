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
    , dynamicExclusions(DynamicExclusions{})
{
    setHiddenFoldersVisible(pHiddenFoldersVisible);
}

Qt::ItemFlags FolderSelectionModel::flags(const QModelIndex &pIndex) const
{
    Qt::ItemFlags lFlags = QFileSystemModel::flags(pIndex);
    lFlags |= Qt::ItemIsUserCheckable;
    if (pIndex.isValid() && mDynamicallyExcludedPaths.contains(filePath(pIndex))) {
        lFlags &= ~Qt::ItemIsEnabled;
    }
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
            if (setContainsSubdir(mExcludedFolders | mDynamicallyExcludedPaths, lPath)) {
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
            if (setContainsSubdir(mExcludedFolders | mDynamicallyExcludedPaths, lPath)) {
                return xi18nc("@info:tooltip %1 is the path of the folder in a listview",
                              "<filename>%1</filename><nl/>will be included in the backup, except "
                              "for unchecked subfolders",
                              filePath(pIndex));
            }
            return xi18nc("@info:tooltip %1 is the path of the folder in a listview",
                          "<filename>%1</filename><nl/>will be included in the backup",
                          filePath(pIndex));
        case StateExcludedAutomatic:
            return xi18nc("@info:tooltip %1 is the path of the folder in a listview",
                          "<filename>%1</filename><nl/>is <emphasis>automatically excluded</emphasis> from the backup."
                          "<nl/>You can control this in the <interface>Advanced</interface> settings.",
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
        recalculateDynamicExclusions();
    }
    emit dataChanged(index(pPath), findLastLeaf(index(pPath)));
}

void FolderSelectionModel::excludePath(const QString &pPath)
{
    QFileInfo lInfo(pPath);
    if (lInfo.isFile()) {
        excludeFile(pPath);
    } else {
        excludeFolder(pPath);
    }
}

void FolderSelectionModel::excludeFolder(const QString &pPath)
{
    const InclusionState lState = inclusionState(pPath);
    if (lState == StateExcluded || lState == StateExcludedAutomatic) {
        return;
    }
    removeSubDirs(pPath);
    if (lState == StateIncludeInherited) {
        mExcludedFolders.insert(pPath);
        emit excludedFolderAdded(pPath);
    }
    const auto lIndex = index(pPath);
    if (lIndex.isValid()) {
        emit dataChanged(lIndex, findLastLeaf(lIndex));
    }
}

void FolderSelectionModel::excludeFile(const QString &pPath)
{
    const InclusionState lState = inclusionState(pPath);
    if (lState == StateExcluded || lState == StateExcludedAutomatic) {
        return;
    }
    if (lState == StateIncludeInherited) {
        mExcludedFiles.insert(pPath);
        emit excludedFileAdded(pPath);
    }
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
    recalculateDynamicExclusions();
}

void FolderSelectionModel::setExcludedPaths(const QSet<QString> &pExcludedPaths)
{
    setExcludedFolders(pExcludedPaths);
}

void FolderSelectionModel::setExcludedFolders(const QSet<QString> &pExcludedFolders)
{
    QSet<QString> lRemoved = mExcludedFolders - pExcludedFolders;
    QSet<QString> lAdded = pExcludedFolders - mExcludedFolders;
    if (lRemoved.count() + lAdded.count() == 0)
        return;

    beginResetModel();
    mExcludedFolders = pExcludedFolders;
    foreach (const QString &lRemovedPath, lRemoved) {
        emit excludedFolderRemoved(lRemovedPath);
    }
    foreach (const QString &lAddedPath, lAdded) {
        emit excludedFolderAdded(lAddedPath);
    }
    endResetModel();
}

void FolderSelectionModel::setExcludedFiles(const QSet<QString> &pExcludedFiles)
{
    QSet<QString> lRemoved = mExcludedFiles - pExcludedFiles;
    QSet<QString> lAdded = pExcludedFiles - mExcludedFiles;
    if (lRemoved.count() + lAdded.count() == 0)
        return;

    mExcludedFiles = pExcludedFiles;
    foreach (const QString &lRemovedPath, lRemoved) {
        emit excludedFileRemoved(lRemovedPath);
    }
    foreach (const QString &lAddedPath, lAdded) {
        emit excludedFileAdded(lAddedPath);
    }
}

QSet<QString> FolderSelectionModel::includedPaths() const
{
    return mIncludedPaths;
}

QSet<QString> FolderSelectionModel::excludedPaths() const
{
    return mExcludedFolders;
}

QSet<QString> FolderSelectionModel::excludedFolders() const
{
    return mExcludedFolders;
}

QSet<QString> FolderSelectionModel::excludedFiles() const
{
    return mExcludedFiles;
}

QSet<QString> FolderSelectionModel::dynamicallyExcludedPaths() const
{
    return mDynamicallyExcludedPaths;
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
    if (mDynamicallyExcludedPaths.contains(pPath)) {
        return StateExcludedAutomatic;
    }
    if (mExcludedFolders.contains(pPath) || mExcludedFiles.contains(pPath)) {
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
    QSet<QString>::iterator it = mExcludedFolders.begin();
    QString lPath = pPath + QStringLiteral("/");
    while (it != mExcludedFolders.end()) {
        if (*it == pPath || it->startsWith(lPath)) {
            QString lPathCopy = *it;
            it = mExcludedFolders.erase(it);
            emit excludedFolderRemoved(lPathCopy);
        } else {
            ++it;
        }
    }
    QSet<QString>::iterator itFiles = mExcludedFiles.begin();
    while (itFiles != mExcludedFiles.end()) {
        if (itFiles->startsWith(lPath)) {
            QString lPathCopy = *itFiles;
            itFiles = mExcludedFiles.erase(itFiles);
            emit excludedFileRemoved(lPathCopy);
        } else {
            ++itFiles;
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

void FolderSelectionModel::recalculateDynamicExclusions()
{
    QStringList lNewDynamicPathList = dynamicExclusions.pathsExcluded(mIncludedPaths.values());
    QSet<QString> lNewDynamicPathSet = QSet(lNewDynamicPathList.begin(), lNewDynamicPathList.end());

    QSet<QString> lRemoved = mDynamicallyExcludedPaths - lNewDynamicPathSet;
    QSet<QString> lAdded = lNewDynamicPathSet - mDynamicallyExcludedPaths;
    if (lRemoved.count() + lAdded.count() == 0)
        return;

    mDynamicallyExcludedPaths = lNewDynamicPathSet;

    foreach (const QString &lRemovedPath, lRemoved) {
        removeSubDirs(lRemovedPath);
        QFileInfo lInfo(lRemovedPath);
        if (lInfo.isFile()) {
            emit excludedFileRemoved(lRemovedPath);
        } else {
            emit excludedFolderRemoved(lRemovedPath);
        }
        const QModelIndex lRemovedIndex = index(lRemovedPath);
        if (lRemovedIndex.isValid()) {
            emit dataChanged(lRemovedIndex, findLastLeaf(lRemovedIndex));
            QModelIndex lRecurseIndex = lRemovedIndex.parent();
            while (lRecurseIndex.isValid()) {
                emit dataChanged(lRecurseIndex, lRecurseIndex);
                lRecurseIndex = lRecurseIndex.parent();
            }
        }
    }
    foreach (const QString &lAddedPath, lAdded) {
        removeSubDirs(lAddedPath);
        QFileInfo lInfo(lAddedPath);
        if (lInfo.isFile()) {
            emit excludedFileAdded(lAddedPath);
        } else {
            emit excludedFolderAdded(lAddedPath);
        }
        const QModelIndex lAddedIndex = index(lAddedPath);
        if (lAddedIndex.isValid()) {
            emit dataChanged(lAddedIndex, findLastLeaf(lAddedIndex));
            QModelIndex lRecurseIndex = lAddedIndex.parent();
            while (lRecurseIndex.isValid()) {
                emit dataChanged(lRecurseIndex, lRecurseIndex);
                lRecurseIndex = lRecurseIndex.parent();
            }
        }
    }
}
