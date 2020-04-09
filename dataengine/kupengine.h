// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL

#ifndef KUPENGINE_H
#define KUPENGINE_H

#include <Plasma/DataEngine>
#include <QLocalSocket>

class KupEngine : public Plasma::DataEngine
{
	Q_OBJECT
public:
	KupEngine(QObject *pParent, const QVariantList &pArgs);
	Plasma::Service *serviceForSource (const QString &pSource) override;

public slots:
//	void refresh();
	void processData();
	void checkConnection(QLocalSocket::LocalSocketState pState);

private:
	void setPlanData(int i, const QJsonObject &pPlan, const QString &pKey);
	void setCommonData(const QJsonObject &pCommonStatus, const QString &pKey);
	QLocalSocket *mSocket;
	QString mSocketName;
};

#endif // KUPENGINE_H
