# SPDX-FileCopyrightText: 2025 Simon Persson <simon.persson@mykolab.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

#this is a library so it needs to enforce it's translation domain, not use the application's domain.
add_definitions(-DTRANSLATION_DOMAIN="kup")
include_directories("../daemon")

set(kupfileitemaction_SRCS
        fileitemaction.cpp
)

kcoreaddons_add_plugin(kupfileitemaction INSTALL_NAMESPACE "kf${QT_MAJOR_VERSION}/kfileitemaction" SOURCES ${kupfileitemaction_SRCS})

target_link_libraries(kupfileitemaction
        KF${QT_MAJOR_VERSION}::I18n
        KF${QT_MAJOR_VERSION}::KIOWidgets
)
