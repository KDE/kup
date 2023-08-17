# SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(bupworker_SRCS
bupworker.cpp
bupvfs.cpp
vfshelpers.cpp
)

ecm_qt_declare_logging_category(bupworker_SRCS
    HEADER kupkio_debug.h
    IDENTIFIER KUPKIO
    CATEGORY_NAME kup.kio
    DEFAULT_SEVERITY Warning
    EXPORT kup
    DESCRIPTION "Kup KIO worker for bup"
)

add_library(kio_bup MODULE ${bupworker_SRCS})
target_link_libraries(kio_bup
Qt::Core
KF${QT_MAJOR_VERSION}::KIOCore
KF${QT_MAJOR_VERSION}::I18n
LibGit2::LibGit2
)

install(TARGETS kio_bup DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf${QT_MAJOR_VERSION}/kio)

add_definitions(-fexceptions)
