/*
 * Copyright 2019 Simon Persson <simon.persson@mykolab.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy 
 * defined in Section 14 of version 3 of the license.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "filedigger.h"
#include "mergedvfs.h"

#include <git2/global.h>

#include <KAboutData>
#include <KLocalizedString>

#include <QApplication>
#include <QDebug>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFile>
#include <QTextStream>

int main(int pArgCount, char **pArgArray) {
	QApplication lApp(pArgCount, pArgArray);
	QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

	KLocalizedString::setApplicationDomain("kup");

	KAboutData lAbout(QStringLiteral("kupfiledigger"), xi18nc("@title", "File Digger"), QStringLiteral("0.7.3"),
	                  i18n("Browser for bup archives."),
	                  KAboutLicense::GPL, i18n("Copyright (C) 2013-2020 Simon Persson"));
	lAbout.addAuthor(i18n("Simon Persson"), i18n("Maintainer"), "simon.persson@mykolab.com");
	lAbout.setTranslator(xi18nc("NAME OF TRANSLATORS", "Your names"), xi18nc("EMAIL OF TRANSLATORS", "Your emails"));
	KAboutData::setApplicationData(lAbout); //this calls qApp.setApplicationName, setVersion, etc.

	QCommandLineParser lParser;
	lParser.addOption(QCommandLineOption(QStringList() << QStringLiteral("b") << QStringLiteral("branch"),
	                                     i18n("Name of the branch to be opened."),
	                                     QStringLiteral("branch name"), QStringLiteral("kup")));
	lParser.addPositionalArgument(QStringLiteral("<repository path>"), i18n("Path to the bup repository to be opened."));

	lAbout.setupCommandLine(&lParser);
	lParser.process(lApp);
	lAbout.processCommandLine(&lParser);

	QString lRepoPath;
	QStringList lPosArgs = lParser.positionalArguments();
	if(!lPosArgs.isEmpty()) {
		lRepoPath = lPosArgs.takeFirst();
	}

	// This needs to be called first thing, before any other calls to libgit2.
	git_libgit2_init();

	auto lFileDigger = new FileDigger(lRepoPath, lParser.value(QStringLiteral("branch")));
	lFileDigger->show();
	int lRetVal = QApplication::exec();
	git_libgit2_shutdown();
	return lRetVal;
}
