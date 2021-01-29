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
#include <QSplitter>
#include <QTimer>

Purger::Purger(QString pRepoPath, QString pBranchName, QWidget *pParent)
    : KMainWindow(pParent), mRepoPath(std::move(pRepoPath)), 
      mBranchName(std::move(pBranchName))
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
	connect(mCollectProcess, &KProcess::readyReadStandardError, [this] {
		mTextEdit->append(QString::fromUtf8(mCollectProcess->readAllStandardError()));
	});
	connect(mCollectProcess, SIGNAL(finished(int,QProcess::ExitStatus)), 
	        SLOT(purgeDone(int,QProcess::ExitStatus)));
	QTimer::singleShot(0, this, [this]{fillListWidget();});
}

QSize Purger::sizeHint() const {
	return {800, 600};
}

void Purger::fillListWidget() {
	if(mRepoPath.isEmpty()) {
		return; //FIXME
	}
	QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	mListWidget->clear();
	KProcess lListProcess;
	lListProcess.setOutputChannelMode(KProcess::SeparateChannels);
	lListProcess << QStringLiteral("bup");
	lListProcess << QStringLiteral("-d") << mRepoPath;
	lListProcess << QStringLiteral("ls") << mBranchName;
	lListProcess.execute();
	KFormat lFormat;
	const auto lSnapshots = QString::fromUtf8(lListProcess.readAllStandardOutput()).split(QRegExp("\\s+"));
	for(const QString &lSnapshot: lSnapshots) {
		if(lSnapshot != QStringLiteral("latest") && !lSnapshot.isEmpty()) {
			auto lDateTime = QDateTime::fromString(lSnapshot, QStringLiteral("yyyy-MM-dd-HHmmss"));
			auto lDisplayText = lFormat.formatRelativeDateTime(lDateTime, QLocale::ShortFormat);
			auto lItem = new QListWidgetItem(lDisplayText, mListWidget);
			lItem->setWhatsThis(lSnapshot); //misuse of field, for later use when removing
			lItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
			lItem->setCheckState(Qt::Unchecked);
		}
	}
	QGuiApplication::restoreOverrideCursor();
}

void Purger::purge() {
	qCInfo(KUPPURGER)  << "Starting purge operation.";
	mDeleteAction->setEnabled(false);
	bool lAnythingRemoved = false;
	for(int i=0; i < mListWidget->count(); ++i) {
		auto lItem = mListWidget->item(i);
		qCInfo(KUPPURGER)  << lItem->text() << lItem->checkState();
		if(lItem->checkState() == Qt::Checked) {
			KProcess lRemoveProcess;
			lRemoveProcess.setOutputChannelMode(KProcess::SeparateChannels);
			lRemoveProcess << QStringLiteral("bup");
			lRemoveProcess << QStringLiteral("-d") << mRepoPath << QStringLiteral("rm");
			lRemoveProcess << QStringLiteral("--unsafe") << QStringLiteral("--verbose");
			lRemoveProcess << QString("%1/%2").arg(mBranchName).arg(lItem->whatsThis());
			qCInfo(KUPPURGER)  << lRemoveProcess.program();
			if(lRemoveProcess.execute() == 0) {
				lAnythingRemoved = true;
			}
			auto lOutput = QString::fromUtf8(lRemoveProcess.readAllStandardError());
			if(!lOutput.isEmpty()) {
				qCInfo(KUPPURGER) << lOutput;
			}
		}
	}
	if(lAnythingRemoved) {
		QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
		qCInfo(KUPPURGER)  << mCollectProcess->program();
		mCollectProcess->start();
	} else {
		mDeleteAction->setEnabled(true);
	}
}

void Purger::purgeDone(int pExitCode, QProcess::ExitStatus pExitStatus) {
	QGuiApplication::restoreOverrideCursor();
	mDeleteAction->setEnabled(true);
	mTextEdit->append(QString::fromUtf8(mCollectProcess->readAllStandardError()));
	fillListWidget();
}

