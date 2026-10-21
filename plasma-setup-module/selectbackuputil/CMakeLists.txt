# SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
#
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

ecm_add_qml_module(plasmasetup_selectbackuputil
    URI "org.kde.plasmasetup.selectbackuputil"
    GENERATE_PLUGIN_SOURCE
    SOURCES
        restorejob.cpp
        externaldrivebackupsmodel.cpp
        externaldrivebackupsmodel.h
)

target_link_libraries(plasmasetup_selectbackuputil PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Qml
    Qt::Concurrent
    KF6::Solid
    KF6::AuthCore
    KF6::CoreAddons
)

ecm_finalize_qml_module(plasmasetup_selectbackuputil)
