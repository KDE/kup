// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "filedigger.h"
#include "mergedvfs.h"

#include <git2/global.h>

#include <KAboutData>
#include <KLocalizedString>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QTextStream>

int main(int pArgCount, char **pArgArray)
{
    QApplication lApp(pArgCount, pArgArray);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    KLocalizedString::setApplicationDomain("kup");

    KAboutData lAbout(QStringLiteral("kupfiledigger"),
                      xi18nc("@title", "File Digger"),
                      QStringLiteral("0.9.1"),
                      i18n("Browser for bup archives."),
                      KAboutLicense::GPL,
                      i18n("Copyright (C) 2013-2020 Simon Persson"));
    lAbout.addAuthor(i18n("Simon Persson"), i18n("Maintainer"), "simon.persson@mykolab.com");
    lAbout.setTranslator(xi18nc("NAME OF TRANSLATORS", "Your names"), xi18nc("EMAIL OF TRANSLATORS", "Your emails"));
    KAboutData::setApplicationData(lAbout); // this calls qApp.setApplicationName, setVersion, etc.

    QCommandLineParser lParser;
    lParser.addOption(QCommandLineOption(QStringList() << QStringLiteral("b") << QStringLiteral("branch"),
                                         i18n("Name of the branch to be opened."),
                                         QStringLiteral("branch name"),
                                         QStringLiteral("kup")));
    lParser.addPositionalArgument(QStringLiteral("<repository path>"), i18n("Path to the bup repository to be opened."));

    lAbout.setupCommandLine(&lParser);
    lParser.process(lApp);
    lAbout.processCommandLine(&lParser);

    QString lRepoPath;
    QStringList lPosArgs = lParser.positionalArguments();
    if (!lPosArgs.isEmpty()) {
        auto lDir = QDir(lPosArgs.takeFirst());
        lRepoPath = lDir.absolutePath();
    }

    // This needs to be called first thing, before any other calls to libgit2.
    git_libgit2_init();

    auto lFileDigger = new FileDigger(lRepoPath, lParser.value(QStringLiteral("branch")));
    lFileDigger->show();
    int lRetVal = QApplication::exec();
    git_libgit2_shutdown();
    return lRetVal;
}
