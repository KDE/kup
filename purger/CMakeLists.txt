# SPDX-FileCopyrightText: 2021 Simon Persson <simon.persson@mykolab.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(purger_SRCS
purger.cpp
main.cpp
)

ecm_qt_declare_logging_category(purger_SRCS
    HEADER kuppurger_debug.h
    IDENTIFIER KUPPURGER
    CATEGORY_NAME kup.purger
    DEFAULT_SEVERITY Warning
    EXPORT kup
    DESCRIPTION "Kup Purger"
)

add_executable(kup-purger ${purger_SRCS})
target_link_libraries(kup-purger
Qt5::Widgets
KF5::I18n
KF5::CoreAddons
KF5::XmlGui
)

########### install files ###############
install(TARGETS kup-purger ${INSTALL_TARGETS_DEFAULT_ARGS})
