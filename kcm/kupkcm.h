// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

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
#if QT_VERSION_MAJOR == 5
    KupKcm(QWidget *pParent, const QVariantList &pArgs);
    QSize sizeHint() const override;
#else
    KupKcm(QObject *parent, const KPluginMetaData &md, const QVariantList &pArgs);
#endif

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
    int mSourcePageToShow;
};

#endif // KUPKCM_H
