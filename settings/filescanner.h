// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
// SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef FILESCANNER_H
#define FILESCANNER_H

#include <QObject>
#include <QSet>
#include <QString>

class QFileInfo;
class QTimer;

class FileScanner : public QObject
{
    Q_OBJECT
public:
    FileScanner(QObject *pParent = nullptr);
    bool event(QEvent *pEvent) override;
    const QSet<QString> &unreadableFiles();
    const QSet<QString> &unreadableFolders();

public slots:
    void includePath(const QString &pPath);
    void excludePath(const QString &pPath);
    void setExclusionRegExps(const QSet<QString> &pExclusionRegExps);

signals:
    void unreadablesChanged(QPair<QSet<QString>, QSet<QString>>);
    void symlinkProblemsChanged(QHash<QString, QString>);
    void includedPathScanFinished(QString folder);
    void scanFinished();

protected slots:
    void sendPendingUnreadables();
    void sendPendingSymlinks();

protected:
    bool isPathIncluded(const QString &pPath);
    void checkPathForProblems(const QFileInfo &pFileInfo);
    bool isSymlinkProblematic(const QString &pTarget);
    void scanFolder(const QString &pPath);

    QSet<QString> mIncludedPaths;
    QSet<QString> mExcludedPaths;

    QSet<QString> mUnreadableFolders;
    QSet<QString> mUnreadableFiles;
    QTimer *mUnreadablesTimer;

    QSet<QRegularExpression> mExclusionRegExps;

    QHash<QString, QString> mSymlinksNotOk;
    QHash<QString, QString> mSymlinksOk;
    QTimer *mSymlinkTimer;

    QHash<QString, uint> mEntriesRemaining;
    QHash<QString, bool> mIncludedPathsRemaining;
};

#endif
