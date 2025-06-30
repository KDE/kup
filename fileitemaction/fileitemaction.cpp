// SPDX-FileCopyrightText: 2025 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
#include "kupdaemon.h"

#include <KAbstractFileItemActionPlugin>
#include <KFileItem>
#include <KFileItemListProperties>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KProcess>

#include <QAction>
#include <QDBusInterface>
#include <QDBusPendingReply>
#include <QEventLoop>
#include <QTimer>
#include <QWidget>

class FileItemAction : public KAbstractFileItemActionPlugin
{
    Q_OBJECT

public:
    explicit FileItemAction(QObject *pParent, const QVariantList &pArgs)
        : KAbstractFileItemActionPlugin(pParent)
    {
        Q_UNUSED(pArgs);
    }

    QList<QAction *> actions(const KFileItemListProperties &pFileItemInfos, QWidget *pParentWidget) override
    {
        auto lFileItems = pFileItemInfos.items();
        if (!pFileItemInfos.isLocal() || lFileItems.isEmpty()) {
            return {};
        }

        QDBusInterface lInterface(KUP_DBUS_SERVICE_NAME, KUP_DBUS_OBJECT_PATH);
        if (!lInterface.isValid()) {
            return {};
        }
        auto lFileItem = lFileItems.front(); // This action only works on the first selected item.
        auto lLocalPath = lFileItem.localPath();
        QDBusPendingCallWatcher lWatcher(lInterface.asyncCall("getRepositoryPath", lLocalPath));
        QEventLoop lLoop;
        QString lRepoPath;

        connect(&lWatcher, &QDBusPendingCallWatcher::finished, this, [&](auto pCall) {
            QDBusPendingReply<QString> lReply = *pCall;
            if (!lReply.isError()) {
                lRepoPath = lReply.value();
            }
            lLoop.quit();
        });

        QTimer::singleShot(100, &lLoop, &QEventLoop::quit);
        lLoop.exec(QEventLoop::ExcludeUserInputEvents);

        if (lRepoPath.isEmpty()) {
            return {};
        }

        QString lDescription = lFileItem.isDir() ? i18n("Restore Missing Files…") : i18n("Restore Previous Version…");
        auto lAction = new QAction(lDescription, pParentWidget);
        connect(lAction, &QAction::triggered, [lLocalPath, lRepoPath] {
            KProcess::startDetached("kup-filedigger", {"--path", lLocalPath, lRepoPath});
        });

        return {lAction};
    }
};

K_PLUGIN_CLASS_WITH_JSON(FileItemAction, "kupfileitemaction.json")

#include "fileitemaction.moc"
