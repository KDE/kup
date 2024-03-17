// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef KUPDAEMON_H
#define KUPDAEMON_H

#include <KSharedConfig>

#define KUP_DBUS_SERVICE_NAME QStringLiteral("org.kde.kupdaemon")
#define KUP_DBUS_OBJECT_PATH QStringLiteral("/DaemonControl")

class KupSettings;
class PlanExecutor;

class KJob;
class KUiServerJobTracker;

class QLocalServer;
class QLocalSocket;
class QSessionManager;
class QTimer;

class KupDaemon : public QObject
{
    Q_OBJECT

public:
    KupDaemon();
    ~KupDaemon() override;
    bool shouldStart();
    void setupGuiStuff();
    void slotShutdownRequest(QSessionManager &pManager);
    void registerJob(KJob *pJob);
    void unregisterJob(KJob *pJob);

public slots:
    void reloadConfig();
    void runIntegrityCheck(const QString &pPath);
    void saveNewBackup(int pPlanNumber);

private:
    void setupExecutors();
    void handleRequests(QLocalSocket *pSocket);
    void sendStatus(QLocalSocket *pSocket);

    KSharedConfigPtr mConfig;
    KupSettings *mSettings;
    QList<PlanExecutor *> mExecutors;
    QTimer *mUsageAccTimer;
    QTimer *mStatusUpdateTimer;
    bool mWaitingToReloadConfig;
    KUiServerJobTracker *mJobTracker;
    QLocalServer *mLocalServer;
    QList<QLocalSocket *> mSockets;
};

#endif /*KUPDAEMON_H*/
