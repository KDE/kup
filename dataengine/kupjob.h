// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef KUPJOB_H
#define KUPJOB_H

#include <QtGlobal>

#if QT_VERSION_MAJOR == 5
#include <Plasma/ServiceJob>
#else
#include <Plasma5Support/ServiceJob>
namespace Plasma = Plasma5Support;
#endif

class QLocalSocket;

class KupJob : public Plasma::ServiceJob
{
    Q_OBJECT

public:
    KupJob(int pPlanNumber, QLocalSocket *pSocket, const QString &pOperation, QMap<QString, QVariant> &pParameters, QObject *pParent = nullptr);

protected:
    void start() override;
    QLocalSocket *mSocket;
    int mPlanNumber;
};

#endif
