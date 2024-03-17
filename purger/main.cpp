// SPDX-FileCopyrightText: 2021 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "purger.h"

#include <KAboutData>
#include <KLocalizedString>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>

int main(int pArgCount, char **pArgArray)
{
    QApplication lApp(pArgCount, pArgArray);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    KLocalizedString::setApplicationDomain("kup");

    KAboutData lAbout(QStringLiteral("kuppurger"),
                      xi18nc("@title", "Purger"),
                      QStringLiteral("0.9.1"),
                      i18n("Purger for bup archives."),
                      KAboutLicense::GPL,
                      i18n("Copyright (C) 2013-2021 Simon Persson"));
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

    QStringList lPosArgs = lParser.positionalArguments();
    if (lPosArgs.isEmpty()) {
        return 1;
    }
    QString lRepoPath = QDir(lPosArgs.takeFirst()).absolutePath();
    if (lRepoPath.isEmpty()) {
        return 1;
    }
    auto lPurger = new Purger(lRepoPath, lParser.value(QStringLiteral("branch")));
    lPurger->show();
    return QApplication::exec();
}
