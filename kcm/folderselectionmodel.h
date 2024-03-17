// SPDX-FileCopyrightText: 2003 Scott Wheeler <wheeler@kde.org>
// SPDX-FileCopyrightText: 2004 Max Howell <max.howell@methylblue.com>
// SPDX-FileCopyrightText: 2004 Mark Kretschmann <markey@web.de>
// SPDX-FileCopyrightText: 2008 Seb Ruiz <ruiz@kde.org>
// SPDX-FileCopyrightText: 2008 Sebastian Trueg <trueg@kde.org>
// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-2.0-only

#ifndef FOLDER_SELECTION_MODEL_H
#define FOLDER_SELECTION_MODEL_H

#include <QFileSystemModel>
#include <QSet>

class FolderSelectionModel : public QFileSystemModel
{
    Q_OBJECT

public:
    explicit FolderSelectionModel(bool pHiddenFoldersVisible = false, QObject *pParent = nullptr);

    enum InclusionState { StateNone, StateIncluded, StateExcluded, StateIncludeInherited, StateExcludeInherited };

    enum CustomRoles { IncludeStateRole = 7777 };

    Qt::ItemFlags flags(const QModelIndex &pIndex) const override;
    QVariant data(const QModelIndex &pIndex, int pRole = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &pIndex, const QVariant &pValue, int pRole = Qt::EditRole) override;

    void setIncludedPaths(const QSet<QString> &pIncludedPaths);
    void setExcludedPaths(const QSet<QString> &pExcludedPaths);
    QSet<QString> includedPaths() const;
    QSet<QString> excludedPaths() const;

    /**
     * Include the specified path. All subdirs will be reset.
     */
    void includePath(const QString &pPath);

    /**
     * Exclude the specified path. All subdirs will be reset.
     */
    void excludePath(const QString &pPath);

    int columnCount(const QModelIndex &) const override
    {
        return 1;
    }

    InclusionState inclusionState(const QModelIndex &pIndex) const;
    InclusionState inclusionState(const QString &pPath) const;

    bool hiddenFoldersVisible() const;

public slots:
    void setHiddenFoldersVisible(bool pVisible);

signals:
    void includedPathAdded(const QString &pPath);
    void excludedPathAdded(const QString &pPath);
    void includedPathRemoved(const QString &pPath);
    void excludedPathRemoved(const QString &pPath);

private:
    QModelIndex findLastLeaf(const QModelIndex &index);
    void removeSubDirs(const QString &path);

    QSet<QString> mIncludedPaths;
    QSet<QString> mExcludedPaths;
};

#endif
