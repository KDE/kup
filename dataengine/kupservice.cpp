// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "kupservice.h"
#include "kupjob.h"

#include <QLocalSocket>

KupService::KupService(int pPlanNumber, QLocalSocket *pSocket, QObject *pParent)
    : Plasma::Service(pParent)
    , mSocket(pSocket)
    , mPlanNumber(pPlanNumber)
{
    setName(QStringLiteral("kupservice"));
}

Plasma::ServiceJob *KupService::createJob(const QString &pOperation, QMap<QString, QVariant> &pParameters)
{
    return new KupJob(mPlanNumber, mSocket, pOperation, pParameters, this);
}

KupDaemonService::KupDaemonService(QLocalSocket *pSocket, QObject *pParent)
    : Plasma::Service(pParent)
    , mSocket(pSocket)
{
    setName(QStringLiteral("kupdaemonservice"));
}

Plasma::ServiceJob *KupDaemonService::createJob(const QString &pOperation, QMap<QString, QVariant> &pParameters)
{
    return new KupJob(-1, mSocket, pOperation, pParameters, this);
}
