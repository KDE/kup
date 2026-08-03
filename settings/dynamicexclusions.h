// SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef DYNAMICEXCLUSIONS_H
#define DYNAMICEXCLUSIONS_H

#include "backupplan.h"

#include <QStringList>

class DynamicExclusions
{
public:
    void setFromPlan(const BackupPlan &plan);

    bool mExcludeTrash{};
    bool mExcludeAppStates{};
    bool mExcludeCaches{};
    bool mExcludeEncryptedMounts{};
    bool mExcludeSnapshots{};
    bool mExcludeContainers{};
    bool mExcludeUserFlatpaks{};

    QStringList pathsExcluded(const QStringList &pPathsIncluded);
};

#endif // DYNAMICEXCLUSIONS_H
