# SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(bupslave_SRCS
bupslave.cpp
bupvfs.cpp
vfshelpers.cpp
)

ecm_qt_declare_logging_category(bupslave_SRCS
    HEADER kupkio_debug.h
    IDENTIFIER KUPKIO
    CATEGORY_NAME kup.kio
    DEFAULT_SEVERITY Warning
    EXPORT kup
    DESCRIPTION "Kup KIO slave for bup"
)

add_library(kio_bup MODULE ${bupslave_SRCS})
target_link_libraries(kio_bup
Qt5::Core
KF5::KIOCore
KF5::I18n
LibGit2::LibGit2
)

install(TARGETS kio_bup DESTINATION ${PLUGIN_INSTALL_DIR}/kf5/kio)

add_definitions(-fexceptions)
