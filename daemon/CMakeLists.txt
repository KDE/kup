# SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

include_directories("../settings")

set(kupdaemon_SRCS
main.cpp
kupdaemon.cpp
planexecutor.cpp
edexecutor.cpp
fsexecutor.cpp
backupjob.cpp
bupjob.cpp
bupverificationjob.cpp
buprepairjob.cpp
rsyncjob.cpp
../settings/backupplan.cpp
../settings/kupsettings.cpp
../settings/kuputils.cpp
)

ecm_qt_declare_logging_category(kupdaemon_SRCS
    HEADER kupdaemon_debug.h
    IDENTIFIER KUPDAEMON
    CATEGORY_NAME kup.daemon
    DEFAULT_SEVERITY Warning
    EXPORT kup
    DESCRIPTION "Kup Daemon"
)

add_executable(kup-daemon ${kupdaemon_SRCS})
target_link_libraries(kup-daemon
Qt::Core
Qt::DBus
Qt::Gui
KF${QT_MAJOR_VERSION}::ConfigCore
KF${QT_MAJOR_VERSION}::KIOCore
KF${QT_MAJOR_VERSION}::KIOFileWidgets
KF${QT_MAJOR_VERSION}::IdleTime
KF${QT_MAJOR_VERSION}::I18n
KF${QT_MAJOR_VERSION}::JobWidgets
KF${QT_MAJOR_VERSION}::Solid
KF${QT_MAJOR_VERSION}::Notifications
KF${QT_MAJOR_VERSION}::CoreAddons
KF${QT_MAJOR_VERSION}::DBusAddons
)

########### install files ###############
install(TARGETS kup-daemon ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES kup-daemon.desktop DESTINATION ${KDE_INSTALL_AUTOSTARTDIR})
install(FILES kupdaemon.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})

