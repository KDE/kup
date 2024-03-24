// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "kupengine.h"
#include "kupdaemon.h"
#include "kupservice.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>

KupEngine::KupEngine(QObject *pParent, const QVariantList &pArgs)
    : Plasma::DataEngine(pParent /* pArgs*/)
{
    Q_UNUSED(pArgs);
    mSocketName = QStringLiteral("kup-daemon-");
    mSocketName += QString::fromLocal8Bit(qgetenv("USER"));
    mSocket = new QLocalSocket(this);
    connect(mSocket, &QLocalSocket::readyRead, this, &KupEngine::processData);
    connect(mSocket, &QLocalSocket::stateChanged, this, &KupEngine::checkConnection);
    // wait 5 seconds before trying to connect first time
    QTimer::singleShot(5000, mSocket, [&] {
        mSocket->connectToServer(mSocketName);
    });
    setData(QStringLiteral("common"), QStringLiteral("plan count"), 0);
}

Plasma::Service *KupEngine::serviceForSource(const QString &pSource)
{
    if (pSource == "daemon") {
        return new KupDaemonService(mSocket, this);
    }

    bool lIntOk;
    int lPlanNumber = pSource.toInt(&lIntOk);
    if (lIntOk) {
        return new KupService(lPlanNumber, mSocket, this);
    }
    return nullptr;
}

void KupEngine::processData()
{
    if (mSocket->bytesAvailable() <= 0) {
        return;
    }
    QJsonDocument lDoc = QJsonDocument::fromJson(mSocket->readAll());
    if (!lDoc.isObject()) {
        return;
    }
    QJsonObject lEvent = lDoc.object();
    if (lEvent["event"] == QStringLiteral("status update")) {
        QJsonArray lPlans = lEvent["plans"].toArray();
        setData(QStringLiteral("common"), QStringLiteral("plan count"), lPlans.count());
        setCommonData(lEvent, QStringLiteral("tray icon active"));
        setCommonData(lEvent, QStringLiteral("tooltip icon name"));
        setCommonData(lEvent, QStringLiteral("tooltip title"));
        setCommonData(lEvent, QStringLiteral("tooltip subtitle"));
        setCommonData(lEvent, QStringLiteral("any plan busy"));
        setCommonData(lEvent, QStringLiteral("no plan reason"));
        for (int i = 0; i < lPlans.count(); ++i) {
            QJsonObject lPlan = lPlans[i].toObject();
            setPlanData(i, lPlan, QStringLiteral("description"));
            setPlanData(i, lPlan, QStringLiteral("destination available"));
            setPlanData(i, lPlan, QStringLiteral("status heading"));
            setPlanData(i, lPlan, QStringLiteral("status details"));
            setPlanData(i, lPlan, QStringLiteral("icon name"));
            setPlanData(i, lPlan, QStringLiteral("log file exists"));
            setPlanData(i, lPlan, QStringLiteral("busy"));
            setPlanData(i, lPlan, QStringLiteral("bup type"));
        }
    }
}

void KupEngine::checkConnection(QLocalSocket::LocalSocketState pState)
{
    if (pState != QLocalSocket::ConnectedState && pState != QLocalSocket::ConnectingState) {
        QTimer::singleShot(10000, mSocket, [&] {
            mSocket->connectToServer(mSocketName);
        });
    }
    if (pState == QLocalSocket::UnconnectedState) {
        setData(QStringLiteral("common"), QStringLiteral("no plan reason"), "");
    }
}

void KupEngine::setPlanData(int i, const QJsonObject &pPlan, const QString &pKey)
{
    setData(QString(QStringLiteral("plan %1")).arg(i), pKey, pPlan[pKey].toVariant());
}

void KupEngine::setCommonData(const QJsonObject &pCommonStatus, const QString &pKey)
{
    setData(QStringLiteral("common"), pKey, pCommonStatus[pKey].toVariant());
}

K_PLUGIN_CLASS_WITH_JSON(KupEngine, "plasma-dataengine-kup.json")

#include "kupengine.moc"
