// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL

#ifndef KUPJOB_H
#define KUPJOB_H

#include <Plasma/ServiceJob>

class QLocalSocket;

class KupJob : public Plasma::ServiceJob
{
	Q_OBJECT

public:
	KupJob(int pPlanNumber, QLocalSocket *pSocket, const QString &pOperation,
	       QMap<QString, QVariant> &pParameters, QObject *pParent = nullptr);

protected:
	void start() override;
	QLocalSocket *mSocket;
	int mPlanNumber;
};

#endif
