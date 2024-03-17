// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "kupjob.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>

KupJob::KupJob(int pPlanNumber, QLocalSocket *pSocket, const QString &pOperation, QMap<QString, QVariant> &pParameters, QObject *pParent)
    : ServiceJob(pParent->objectName(), pOperation, pParameters, pParent)
    , mSocket(pSocket)
    , mPlanNumber(pPlanNumber)
{
}

void KupJob::start()
{
    if (mSocket->state() != QLocalSocket::ConnectedState) {
        return;
    }
    QJsonObject lCommand;
    lCommand["plan number"] = mPlanNumber;
    lCommand["operation name"] = operationName();
    QJsonDocument lDoc(lCommand);
    mSocket->write(lDoc.toJson());
    setResult(false);
}
