// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
// SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "filescanner.h"

#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTimer>

enum class FileScannerEvent {
    ScanFolder = QEvent::User + 1,
    FolderEntryFinished,
};

class ScanFolderEvent : public QEvent
{
public:
    explicit ScanFolderEvent(QString pPath)
        : QEvent(eventType)
        , mPath(std::move(pPath))
    {
    }
    QString mPath;
    static const QEvent::Type eventType = static_cast<QEvent::Type>(FileScannerEvent::ScanFolder);
};

class FolderEntryFinishedEvent : public QEvent
{
public:
    explicit FolderEntryFinishedEvent(QString pParentPath, QString pPath)
        : QEvent(eventType)
        , mParentPath(std::move(pParentPath))
        , mPath(std::move(pPath))
    {
    }
    QString mParentPath;
    QString mPath;
    static const QEvent::Type eventType = static_cast<QEvent::Type>(FileScannerEvent::FolderEntryFinished);
};

FileScanner::FileScanner(QObject *pParent)
    : QObject(pParent)
{
    // create a timer that will call a slot to send the pending updates to UI, one second
    // after the last update comes in, just to minimize risk of showing incomplete
    // information to the user.
    mUnreadablesTimer = new QTimer(this);
    mUnreadablesTimer->setSingleShot(true);
    mUnreadablesTimer->setInterval(1000);
    connect(mUnreadablesTimer, &QTimer::timeout, this, &FileScanner::sendPendingUnreadables);

    mSymlinkTimer = new QTimer(this);
    mSymlinkTimer->setSingleShot(true);
    mSymlinkTimer->setInterval(1000);
    connect(mSymlinkTimer, &QTimer::timeout, this, &FileScanner::sendPendingSymlinks);
}

bool FileScanner::event(QEvent *pEvent)
{
    if (pEvent->type() == ScanFolderEvent::eventType) {
        auto lEvent = dynamic_cast<ScanFolderEvent *>(pEvent);
        if (isPathIncluded(lEvent->mPath)) {
            scanFolder(lEvent->mPath);
        } else {
            QString lParentPath = QFileInfo(lEvent->mPath).absolutePath();
            QCoreApplication::postEvent(this, new FolderEntryFinishedEvent(lParentPath, lEvent->mPath));
        }
        return true;
    } else if (pEvent->type() == FolderEntryFinishedEvent::eventType) {
        auto lEvent = dynamic_cast<FolderEntryFinishedEvent *>(pEvent);
        mEntriesRemaining[lEvent->mParentPath] -= 1;
        if (mEntriesRemaining[lEvent->mParentPath] == 0) {
            const auto lParentOfParentPath = QFileInfo(lEvent->mParentPath).absolutePath();
            QCoreApplication::postEvent(this, new FolderEntryFinishedEvent(lParentOfParentPath, lEvent->mParentPath));
        }
        if (mIncludedPaths.contains(lEvent->mPath)) {
            mIncludedPathsRemaining[lEvent->mPath] = false;
            Q_EMIT includedPathScanFinished(lEvent->mPath);
            if (std::all_of(mIncludedPathsRemaining.constKeyValueBegin(), mIncludedPathsRemaining.constKeyValueEnd(), [](const auto &pair) {
                    return pair.second == false;
                })) {
                Q_EMIT scanFinished();
            }
        }
        return true;
    }

    return QObject::event(pEvent);
}

void FileScanner::scanFolder(const QString &pPath)
{
    QDir lDir(pPath);
    if (!lDir.isReadable()) {
        mUnreadableFolders += pPath;
        mUnreadablesTimer->start();
        QCoreApplication::postEvent(this, new FolderEntryFinishedEvent(QFileInfo(pPath).absolutePath(), pPath));
    } else {
        QFileInfoList lInfoList = lDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
        mEntriesRemaining[pPath] = lInfoList.size();
        foreach (const QFileInfo &lFileInfo, lInfoList) {
            checkPathForProblems(lFileInfo);
        }
        if (lInfoList.size() == 0) {
            QCoreApplication::postEvent(this, new FolderEntryFinishedEvent(QFileInfo(pPath).absolutePath(), pPath));
        }
    }
}

void FileScanner::checkPathForProblems(const QFileInfo &pFileInfo)
{
    if (pFileInfo.isSymLink()) {
        if (isSymlinkProblematic(pFileInfo.symLinkTarget())) {
            mSymlinksNotOk.insert(pFileInfo.absoluteFilePath(), pFileInfo.symLinkTarget());
            mSymlinkTimer->start();
        } else {
            mSymlinksOk.insert(pFileInfo.absoluteFilePath(), pFileInfo.symLinkTarget());
        }
        QCoreApplication::postEvent(this, new FolderEntryFinishedEvent(pFileInfo.absolutePath(), pFileInfo.absoluteFilePath()));
    } else if (pFileInfo.isDir()) {
        QCoreApplication::postEvent(this, new ScanFolderEvent(pFileInfo.absoluteFilePath()), Qt::LowEventPriority);
    } else {
        if (!pFileInfo.isReadable() && isPathIncluded(pFileInfo.absoluteFilePath())) {
            mUnreadableFiles += pFileInfo.absoluteFilePath();
            mUnreadablesTimer->start();
        }
        QCoreApplication::postEvent(this, new FolderEntryFinishedEvent(pFileInfo.absolutePath(), pFileInfo.absoluteFilePath()));
    }
}

const QSet<QString> &FileScanner::unreadableFiles()
{
    return mUnreadableFiles;
}

const QSet<QString> &FileScanner::unreadableFolders()
{
    return mUnreadableFolders;
}

void FileScanner::includePath(const QString &pPath)
{
    if (!mExcludedPaths.remove(pPath)) {
        mIncludedPaths += pPath;
    }
    mIncludedPathsRemaining[pPath] = true;
    checkPathForProblems(QFileInfo(pPath));

    QMutableHashIterator<QString, QString> i(mSymlinksNotOk);
    while (i.hasNext()) {
        i.next();
        if (isPathIncluded(i.value())) {
            mSymlinksOk.insert(i.key(), i.value());
            i.remove();
            mSymlinkTimer->start();
        }
    }
}

void FileScanner::excludePath(const QString &pPath)
{
    if (!mIncludedPaths.remove(pPath)) {
        mExcludedPaths += pPath;
    }

    QFileInfo lInfo(pPath);
    if (lInfo.isDir()) {
        QString lPath = pPath + QStringLiteral("/");
        QSet<QString>::iterator it = mUnreadableFiles.begin();
        while (it != mUnreadableFiles.end()) {
            if (it->startsWith(lPath)) {
                mUnreadablesTimer->start();
                it = mUnreadableFiles.erase(it);
            } else {
                ++it;
            }
        }
        it = mUnreadableFolders.begin();
        while (it != mUnreadableFolders.end()) {
            if (it->startsWith(lPath) || *it == pPath) {
                mUnreadablesTimer->start();
                it = mUnreadableFolders.erase(it);
            } else {
                ++it;
            }
        }
    } else if (lInfo.isFile()) {
        QSet<QString>::iterator it = mUnreadableFiles.begin();
        while (it != mUnreadableFiles.end()) {
            if (*it == pPath) {
                mUnreadablesTimer->start();
                it = mUnreadableFiles.erase(it);
            } else {
                ++it;
            }
        }
    }

    QMutableHashIterator<QString, QString> i(mSymlinksNotOk);
    while (i.hasNext()) {
        if (!isPathIncluded(i.next().key())) {
            i.remove();
            mSymlinkTimer->start();
        }
    }

    i = mSymlinksOk;
    while (i.hasNext()) {
        i.next();
        if (!isPathIncluded(i.key())) {
            i.remove();
        } else if (isSymlinkProblematic(i.value())) {
            mSymlinksNotOk.insert(i.key(), i.value());
            mSymlinkTimer->start();
            i.remove();
        }
    }
}

void FileScanner::sendPendingUnreadables()
{
    emit unreadablesChanged(QPair<QSet<QString>, QSet<QString>>(mUnreadableFolders, mUnreadableFiles));
}

void FileScanner::sendPendingSymlinks()
{
    emit symlinkProblemsChanged(mSymlinksNotOk);
}

bool FileScanner::isPathIncluded(const QString &pPath)
{
    for (const auto &lRegExp : mExclusionRegExps) {
        if (lRegExp.match(pPath).hasMatch()) {
            return false;
        }
    }

    QFileInfo lInfo(pPath);
    if (lInfo.isFile() && mExcludedPaths.contains(pPath)) {
        return false;
    }

    int lLongestInclude = 0;
    foreach (const QString &lPath, mIncludedPaths) {
        bool lMatches = pPath == lPath || pPath.startsWith(lPath + QStringLiteral("/"));
        if (lMatches && lPath.length() > lLongestInclude) {
            lLongestInclude = lPath.length();
        }
    }
    int lLongestExclude = 0;
    foreach (const QString &lPath, mExcludedPaths) {
        bool lMatches = pPath == lPath || pPath.startsWith(lPath + QStringLiteral("/"));
        if (lMatches && lPath.length() > lLongestExclude) {
            lLongestExclude = lPath.length();
        }
    }
    return lLongestInclude > lLongestExclude;
}

bool FileScanner::isSymlinkProblematic(const QString &pTarget)
{
    QFileInfo lTargetInfo(pTarget);
    return lTargetInfo.exists() && !isPathIncluded(pTarget) && pTarget.startsWith(QStringLiteral("/home/"));
}

void FileScanner::setExclusionRegExps(const QSet<QString> &pExclusionRegExps)
{
    mExclusionRegExps.clear();
    for (const auto &lRegExpPattern : pExclusionRegExps) {
        QRegularExpression lRegExp(lRegExpPattern);
        if (lRegExp.isValid()) {
            mExclusionRegExps << lRegExp;
        }
    }
}
