# SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

include_directories("../settings")
include_directories("../daemon")

set(kcm_kup_SRCS
backupplanwidget.cpp
dirselector.cpp
driveselection.cpp
driveselectiondelegate.cpp
folderselectionmodel.cpp
kupkcm.cpp
planstatuswidget.cpp
kbuttongroup.cpp
../settings/backupplan.cpp
../settings/kupsettings.cpp
../settings/kuputils.cpp
)

kcoreaddons_add_plugin(kcm_kup SOURCES ${kcm_kup_SRCS} INSTALL_NAMESPACE "plasma/kcms/systemsettings_qwidgets")

#this is a library so it needs to enforce it's translation domain, not use the application's domain.
add_definitions(-DTRANSLATION_DOMAIN="kup")

target_link_libraries(kcm_kup
Qt::Core
Qt::DBus
Qt::Gui
KF${QT_MAJOR_VERSION}::CoreAddons
KF${QT_MAJOR_VERSION}::ConfigCore
KF${QT_MAJOR_VERSION}::KIOCore
KF${QT_MAJOR_VERSION}::KIOFileWidgets
KF${QT_MAJOR_VERSION}::Solid
KF${QT_MAJOR_VERSION}::I18n
KF${QT_MAJOR_VERSION}::WidgetsAddons
KF${QT_MAJOR_VERSION}::ConfigWidgets
)

if(QT_MAJOR_VERSION STREQUAL "6")
    target_link_libraries(kcm_kup KF${QT_MAJOR_VERSION}::KCMUtils)
endif()

kcmutils_generate_desktop_file(kcm_kup)
