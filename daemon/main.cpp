// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "kupdaemon.h"
#include "kupdaemon_debug.h"

#include <KAboutData>
#include <KDBusService>
#include <KLocalizedString>

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QApplication lApp(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setQuitLockEnabled(false);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    KLocalizedString::setApplicationDomain("kup");

    qCDebug(KUPDAEMON) << "Running Kup daemon...";

    auto *lDaemon = new KupDaemon();
    if (!lDaemon->shouldStart()) {
        qCCritical(KUPDAEMON) << xi18nc("@info:shell Error message at startup",
                                        "Kup is not enabled, enable it from the "
                                        "system settings module. You can do that by running "
                                        "<command>kcmshell5 kup</command>");
        return 0;
    }

    KAboutData lAbout(QStringLiteral("kupdaemon"),
                      xi18nc("@title", "Kup Daemon"),
                      QStringLiteral("0.9.1"),
                      i18n("Kup is a flexible backup solution using the backup storage system 'bup'. "
                           "This allows it to quickly perform incremental backups, only saving the "
                           "parts of files that has actually changed since last backup was saved."),
                      KAboutLicense::GPL,
                      i18n("Copyright (C) 2011-2020 Simon Persson"));
    lAbout.addAuthor(i18n("Simon Persson"), i18n("Maintainer"), "simon.persson@mykolab.com");
    lAbout.setTranslator(xi18nc("NAME OF TRANSLATORS", "Your names"), xi18nc("EMAIL OF TRANSLATORS", "Your emails"));
    KAboutData::setApplicationData(lAbout);

    QCommandLineParser lParser;
    lAbout.setupCommandLine(&lParser);
    lParser.process(lApp);
    lAbout.processCommandLine(&lParser);

    // This call will exit() if an instance is already running
    KDBusService lService(KDBusService::Unique);

    lDaemon->setupGuiStuff();
    KupDaemon::connect(&lApp, &QApplication::commitDataRequest, lDaemon, [lDaemon](QSessionManager &pManager) {
        lDaemon->slotShutdownRequest(pManager);
    });

    return QApplication::exec();
}
