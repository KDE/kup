# SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.16)

set(KF_MIN_VERSION "5.96.0")
set(KDE_COMPILERSETTINGS_LEVEL "5.84.0")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

project(kup)

find_package(LibGit2 REQUIRED)

add_definitions(-DQT_NO_URL_CAST_FROM_STRING)

include(KDEInstallDirs)
include(KDEClangFormat)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDEGitCommitHooks)

include(FeatureSummary)
include(ECMInstallIcons)
include(ECMQtDeclareLoggingCategory)

kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)

find_package(Qt${QT_MAJOR_VERSION} REQUIRED COMPONENTS Core Widgets)
find_package(KF${QT_MAJOR_VERSION} ${KF_MIN_VERSION} REQUIRED COMPONENTS
Solid
KIO
IdleTime
I18n
Notifications
CoreAddons
DBusAddons
Config
JobWidgets
WidgetsAddons
XmlGui
KCMUtils
)


if(QT_MAJOR_VERSION STREQUAL "6")
    find_package(Plasma REQUIRED)
    find_package(Plasma5Support REQUIRED)
    plasma_install_package(plasmoid-qt6 org.kde.kupapplet)
else()
    find_package(KF5Plasma REQUIRED)
    plasma_install_package(plasmoid-qt5 org.kde.kupapplet)
endif()

add_subdirectory(daemon)
add_subdirectory(dataengine)
add_subdirectory(icons)
add_subdirectory(filedigger)
add_subdirectory(kcm)
add_subdirectory(kioworker)
add_subdirectory(purger)

ecm_qt_install_logging_categories(EXPORT kup DESTINATION "${KDE_INSTALL_LOGGINGCATEGORIESDIR}")
ki18n_install(po)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/org.kde.kup.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR})

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES *.cpp *.h *.hpp *.c)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})
