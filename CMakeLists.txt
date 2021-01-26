# SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0063 NEW)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

project(kup)

find_package(LibGit2 REQUIRED)

add_definitions(-DQT_NO_URL_CAST_FROM_STRING)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)
include(FeatureSummary)
include(ECMInstallIcons)
include(ECMQtDeclareLoggingCategory)

find_package(Qt5 REQUIRED COMPONENTS Widgets)
find_package(KF5 REQUIRED COMPONENTS
Solid
KIO
IdleTime
I18n
Notifications
CoreAddons
DBusAddons
Config
Init # needed for the kdeinit cmake macro
JobWidgets
Plasma
WidgetsAddons
)

add_subdirectory(daemon)
add_subdirectory(dataengine)
add_subdirectory(icons)
add_subdirectory(filedigger)
add_subdirectory(kcm)
add_subdirectory(kioslave)

ecm_qt_install_logging_categories(EXPORT kup DESTINATION "${KDE_INSTALL_LOGGINGCATEGORIESDIR}")
plasma_install_package(plasmoid org.kde.kupapplet)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/org.kde.kup.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR})

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

