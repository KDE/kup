# SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
#
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

kauth_install_actions(org.kde.plasmasetup.restorebackup org.kde.plasmasetup.restorebackup.actions)
add_executable(plasma-setup-module-restore-backup-auth-helper
    authhelper.cpp
    authhelper.h
)
target_link_libraries(plasma-setup-module-restore-backup-auth-helper
    Qt::DBus
    KF6::AuthCore
)
install(TARGETS plasma-setup-module-restore-backup-auth-helper DESTINATION ${KAUTH_HELPER_INSTALL_DIR})

kauth_install_helper_files(plasma-setup-module-restore-backup-auth-helper org.kde.plasmasetup.restorebackup root)

install(
    FILES "plasma-setup-module-restore-backup.rules"
    DESTINATION ${KDE_INSTALL_DATADIR}/polkit-1/rules.d
)
