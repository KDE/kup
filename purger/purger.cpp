// SPDX-FileCopyrightText: 2021 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "purger.h"
#include "kuppurger_debug.h"

#include <KFormat>
#include <KLocalizedString>
#include <KStandardAction>
#include <KToolBar>

#include <QDateTime>
#include <QGuiApplication>
#include <QHash>
#include <QSplitter>

Purger::Purger(QString pRepoPath, QString pBranchName, QWidget *pParent)
    : KMainWindow(pParent)
    , mRepoPath(std::move(pRepoPath))
    , mBranchName(std::move(pBranchName))
{
    setWindowIcon(QIcon::fromTheme(QStringLiteral("kup")));
    KToolBar *lAppToolBar = toolBar();
    lAppToolBar->addAction(KStandardAction::quit(this, SLOT(close()), this));
    mDeleteAction = KStandardAction::deleteFile(this, SLOT(purge()), this);
    lAppToolBar->addAction(mDeleteAction);

    mListWidget = new QListWidget();
    auto lSplitter = new QSplitter();
    lSplitter->addWidget(mListWidget);
    mTextEdit = new QTextEdit();
    mTextEdit->setReadOnly(true);
    lSplitter->addWidget(mTextEdit);
    setCentralWidget(lSplitter);

    mCollectProcess = new KProcess();
    mCollectProcess->setOutputChannelMode(KProcess::SeparateChannels);
    *mCollectProcess << QStringLiteral("bup");
    *mCollectProcess << QStringLiteral("-d") << mRepoPath;
    *mCollectProcess << QStringLiteral("gc") << QStringLiteral("--unsafe") << QStringLiteral("--verbose");
    connect(mCollectProcess, &KProcess::readyReadStandardError, this, [this] {
        auto lLogText = QString::fromUtf8(mCollectProcess->readAllStandardError());
        qCInfo(KUPPURGER) << lLogText;
        mTextEdit->append(lLogText);
    });
    connect(mCollectProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &Purger::purgeDone);

    mListProcess = new KProcess();
    mListProcess->setOutputChannelMode(KProcess::SeparateChannels);
    *mListProcess << QStringLiteral("bup");
    *mListProcess << QStringLiteral("-d") << mRepoPath;
    *mListProcess << QStringLiteral("ls") << QStringLiteral("--hash") << mBranchName;
    connect(mListProcess, &KProcess::readyReadStandardOutput, this, [this] {
        KFormat lFormat;
        const auto lLines = QString::fromUtf8(mListProcess->readAllStandardOutput()).split(QChar::LineFeed);
        for (const QString &lLine : lLines) {
            qCDebug(KUPPURGER) << lLine;
            const auto lHash = lLine.left(40);
            if (!lHash.isEmpty() && lHash != QStringLiteral("0000000000000000000000000000000000000000")) {
                const auto lTimeStamp = lLine.mid(41);
                if (mHashes.contains(lHash)) {
                    auto lItem = mHashes.value(lHash);
                    lItem->setWhatsThis(lItem->whatsThis() + QChar::LineFeed + lTimeStamp);
                } else {
                    const auto lDateTime = QDateTime::fromString(lTimeStamp, QStringLiteral("yyyy-MM-dd-HHmmss"));
                    const auto lDisplayText = lFormat.formatRelativeDateTime(lDateTime, QLocale::ShortFormat);
                    auto lItem = new QListWidgetItem(lDisplayText, mListWidget);
                    lItem->setWhatsThis(lTimeStamp); // misuse of field, for later use when removing
                    lItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
                    lItem->setCheckState(Qt::Unchecked);
                    mHashes.insert(lHash, lItem);
                }
            }
        }
    });
    connect(mListProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &Purger::listDone);

    fillListWidget();
}

QSize Purger::sizeHint() const
{
    return {800, 600};
}

void Purger::fillListWidget()
{
    QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    mListWidget->clear();
    mHashes.clear();
    mDeleteAction->setEnabled(false);
    mListProcess->start();
}

void Purger::listDone(int, QProcess::ExitStatus)
{
    QGuiApplication::restoreOverrideCursor();
    mDeleteAction->setEnabled(true);
}

void Purger::purge()
{
    qCInfo(KUPPURGER) << "Starting purge operation.";
    mDeleteAction->setEnabled(false);
    bool lAnythingRemoved = false;
    for (int i = 0; i < mListWidget->count(); ++i) {
        auto lItem = mListWidget->item(i);
        qCInfo(KUPPURGER) << lItem->text() << lItem->whatsThis() << lItem->checkState();
        if (lItem->checkState() == Qt::Checked) {
            const auto lTimeStamps = lItem->whatsThis().split(QChar::LineFeed);
            for (const QString &lTimeStamp : lTimeStamps) {
                KProcess lRemoveProcess;
                lRemoveProcess.setOutputChannelMode(KProcess::SeparateChannels);
                lRemoveProcess << QStringLiteral("bup");
                lRemoveProcess << QStringLiteral("-d") << mRepoPath << QStringLiteral("rm");
                lRemoveProcess << QStringLiteral("--unsafe") << QStringLiteral("--verbose");
                lRemoveProcess << QString("%1/%2").arg(mBranchName, lTimeStamp);
                qCInfo(KUPPURGER) << lRemoveProcess.program();
                if (lRemoveProcess.execute() == 0) {
                    lAnythingRemoved = true;
                    qCInfo(KUPPURGER) << "Successfully removed snapshot";
                }
                const auto lLogText = QString::fromUtf8(lRemoveProcess.readAllStandardError());
                qCInfo(KUPPURGER) << lLogText;
                mTextEdit->append(lLogText);
            }
        }
    }
    if (lAnythingRemoved) {
        QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        qCInfo(KUPPURGER) << mCollectProcess->program();
        mCollectProcess->start();
    } else {
        mDeleteAction->setEnabled(true);
    }
}

void Purger::purgeDone(int pExitCode, QProcess::ExitStatus pExitStatus)
{
    qCInfo(KUPPURGER) << pExitCode << pExitStatus;
    QGuiApplication::restoreOverrideCursor();
    mDeleteAction->setEnabled(true);
    const auto lLogText = QString::fromUtf8(mCollectProcess->readAllStandardError());
    qCInfo(KUPPURGER) << lLogText;
    mTextEdit->append(lLogText);
    fillListWidget();
}
