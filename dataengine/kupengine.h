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
