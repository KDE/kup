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

#ifndef PLANSTATUSWIDGET_H
#define PLANSTATUSWIDGET_H

#include <QGroupBox>

class BackupPlan;
class KupSettings;

class QPushButton;

class QLabel;

class PlanStatusWidget: public QGroupBox
{
	Q_OBJECT
public:
	explicit PlanStatusWidget(BackupPlan *pPlan, QWidget *pParent = 0);
	BackupPlan *plan() const {return mPlan;}

	BackupPlan *mPlan;
	QLabel *mDescriptionLabel;
	QLabel *mStatusIconLabel;
	QLabel *mStatusTextLabel;

public slots:
	void updateIcon();

signals:
	void removeMe();
	void configureMe();
	void duplicateMe();
};


#endif // PLANSTATUSWIDGET_H
