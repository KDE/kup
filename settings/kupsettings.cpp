// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "kupsettings.h"

#include "backupplan.h"
#include <utility>

KupSettings::KupSettings(KSharedConfigPtr pConfig, QObject *pParent)
    : KCoreConfigSkeleton(std::move(pConfig), pParent)
{
    setCurrentGroup(QStringLiteral("Kup settings"));
    addItemBool(QStringLiteral("Backups enabled"), mBackupsEnabled);
    addItemInt(QStringLiteral("Number of backups"), mNumberOfPlans, 0);
}
