# SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
#
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

ecm_add_qml_module(plasmasetup_selectbackup
    URI "org.kde.plasmasetup.selectbackup"
    GENERATE_PLUGIN_SOURCE
    DEPENDENCIES
        Qt::Core
)

ecm_finalize_qml_module(plasmasetup_selectbackup)

kpackage_install_package(. org.kde.plasmasetup.selectbackup packages plasma)
