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

#ifndef KUPKCM_H
#define KUPKCM_H

#include <KCModule>
#include <KSharedConfig>

class BackupPlan;
class BackupPlanWidget;
class KupSettings;
class KupSettingsWidget;
class PlanStatusWidget;

class KAssistantDialog;
class KPageWidgetItem;
class QPushButton;

class QCheckBox;
class QStackedLayout;
class QVBoxLayout;

class KupKcm : public KCModule
{
	Q_OBJECT
public:
	KupKcm(QWidget *pParent, const QVariantList &pArgs);
	QSize sizeHint() const override;

public slots:
	void load() override;
	void save() override;

	void updateChangedStatus();
	void showFrontPage();

private:
	void createSettingsFrontPage();
	void createPlanWidgets(int pIndex);
	void completelyRemovePlan(int pIndex);
	void partiallyRemovePlan(int pIndex);

	KSharedConfigPtr mConfig;
	KupSettings *mSettings;
	QWidget *mFrontPage{};
	QList<BackupPlan *> mPlans;
	QList<BackupPlanWidget *> mPlanWidgets;
	QList<PlanStatusWidget *> mStatusWidgets;
	QList<KConfigDialogManager *> mConfigManagers;
	QStackedLayout *mStackedLayout;
	QVBoxLayout *mVerticalLayout{};
	QCheckBox *mEnableCheckBox{};
	QString mBupVersion;
	QString mRsyncVersion;
	bool mPar2Available;
};

#endif // KUPKCM_H
