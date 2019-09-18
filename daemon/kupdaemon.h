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
	void runIntegrityCheck(const QString& pPath);

private:
	void setupExecutors();
	void handleRequests(QLocalSocket *pSocket);
	void sendStatus(QLocalSocket *pSocket);

	KSharedConfigPtr mConfig;
	KupSettings *mSettings;
	QList<PlanExecutor *> mExecutors;
	QTimer *mUsageAccTimer{};
	QTimer *mStatusUpdateTimer{};
	bool mWaitingToReloadConfig;
	KUiServerJobTracker *mJobTracker;
	QLocalServer *mLocalServer;
	QList<QLocalSocket *> mSockets;
};

#endif /*KUPDAEMON_H*/
