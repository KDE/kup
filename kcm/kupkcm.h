// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef KUPKCM_H
#define KUPKCM_H

#include <KQuickManagedConfigModule>
#include <KSharedConfig>

#include "backupplan.h"
#include "kupsettings.h"

class KupKcm : public KQuickManagedConfigModule
{
    Q_OBJECT
    Q_PROPERTY(KupSettings *settings READ settings CONSTANT)
    Q_PROPERTY(QList<BackupPlan *> plans READ plans NOTIFY plansChanged)
public:
    KupKcm(QObject *parent, const KPluginMetaData &md, const QVariantList &pArgs);

    // void updateChangedStatus();
    // void showFrontPage();

    KupSettings *settings() const;
    QList<BackupPlan *> plans() const;

    Q_INVOKABLE void duplicate(int row);
    Q_INVOKABLE void remove(int row);

    void load() override;
    void save() override;
    bool isSaveNeeded() const override;

Q_SIGNALS:
    void plansChanged();

private:
    void createPlans();
    // void createSettingsFrontPage();
    // void createPlanWidgets(int pIndex);
    void completelyRemovePlan(int pIndex);
    void partiallyRemovePlan(int pIndex);

    KSharedConfigPtr mConfig;
    KupSettings *mSettings = nullptr;
    QList<BackupPlan *> mPlans;
    QList<int> mRemovedPlanNumbers;
    QString mBupVersion;
    QString mRsyncVersion;
    bool mPar2Available;
};

#endif // KUPKCM_H
