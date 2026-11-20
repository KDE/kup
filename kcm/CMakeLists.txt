# SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

include_directories("../settings")
include_directories("../daemon")

add_definitions(-DTRANSLATION_DOMAIN="kup")

kcmutils_add_qml_kcm(kcm_kup SOURCES
    kupkcm.cpp
    ../settings/backupplan.cpp
    ../settings/kuputils.cpp
)

kconfig_target_kcfg_file(kcm_kup
    FILE ${CMAKE_CURRENT_SOURCE_DIR}/../settings/kupsettings.kcfg
    CLASS_NAME KupSettings
    MUTATORS
    DEFAULT_VALUE_GETTERS
    GENERATE_PROPERTIES
    PARENT_IN_CONSTRUCTOR
    GENERATE_MOC
)

kconfig_target_kcfg_file(kcm_kup
    FILE ${CMAKE_CURRENT_SOURCE_DIR}/../settings/backupplansettings.kcfg
    CLASS_NAME BackupPlanSettings
    MUTATORS
    GENERATE_PROPERTIES
    PARENT_IN_CONSTRUCTOR
    GENERATE_MOC
)

# set(kcm_kup_SRCS
# backupplanwidget.cpp
# dirselector.cpp
# driveselection.cpp
# driveselectiondelegate.cpp
# folderselectionmodel.cpp
# planstatuswidget.cpp
# kbuttongroup.cpp
# ../settings/kupsettings.cpp
# ../settings/dynamicexclusions.cpp
# )


target_link_libraries(kcm_kup
    Qt::Core
    Qt::Qml
    Qt::DBus
    Qt::Gui
    KF6::CoreAddons
    KF6::ConfigCore
    KF6::ConfigGui
    KF6::KIOCore
    KF6::KIOFileWidgets
    KF6::Solid
    KF6::I18n
    KF6::WidgetsAddons
    KF6::ConfigWidgets
    KF6::KCMUtils
)
