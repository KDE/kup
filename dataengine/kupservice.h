// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef KUPSERVICE_H
#define KUPSERVICE_H

#include <Plasma/Service>
#include <Plasma/ServiceJob>

using namespace Plasma;

class QLocalSocket;

class KupService : public Plasma::Service
{
	Q_OBJECT

public:
	 KupService(int pPlanNumber, QLocalSocket *pSocket, QObject *pParent = nullptr);
	 ServiceJob *createJob(const QString &pOperation, QMap<QString, QVariant> &pParameters) override;

protected:
	 QLocalSocket *mSocket;
	 int mPlanNumber;
};

class KupDaemonService : public Plasma::Service
{
	Q_OBJECT

public:
	 KupDaemonService(QLocalSocket *pSocket, QObject *pParent = nullptr);
	 ServiceJob *createJob(const QString &pOperation, QMap<QString, QVariant> &pParameters) override;

protected:
	 QLocalSocket *mSocket;
};

#endif // KUPSERVICE_H
