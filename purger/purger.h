// SPDX-FileCopyrightText: 2021 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef PURGER_H
#define PURGER_H

#include <KMainWindow>
#include <KProcess>
#include <QListWidget>
#include <QTextEdit>
#include <QUrl>

class Purger : public KMainWindow
{
    Q_OBJECT
public:
    explicit Purger(QString pRepoPath, QString pBranchName, QWidget *pParent = nullptr);
    QSize sizeHint() const override;

protected slots:
    void fillListWidget();
    void listDone(int, QProcess::ExitStatus);
    void purge();
    void purgeDone(int, QProcess::ExitStatus);

protected:
    QListWidget *mListWidget{};
    QTextEdit *mTextEdit{};
    KProcess *mCollectProcess{};
    KProcess *mListProcess{};
    QHash<QString, QListWidgetItem *> mHashes;
    QAction *mDeleteAction{};
    QString mRepoPath;
    QString mBranchName;
};

#endif // PURGER_H
