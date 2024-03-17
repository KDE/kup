// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef KUPSETTINGS_H
#define KUPSETTINGS_H

#include <KCoreConfigSkeleton>
#include <KSharedConfig>

class KupSettings : public KCoreConfigSkeleton
{
    Q_OBJECT
public:
    explicit KupSettings(KSharedConfigPtr pConfig, QObject *pParent = nullptr);

    // Common enable of backup plans
    bool mBackupsEnabled{};

    // Number of backup plans configured
    int mNumberOfPlans{};
};

#endif // KUPSETTINGS_H
