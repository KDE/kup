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
Qt::Widgets
KF${QT_MAJOR_VERSION}::I18n
KF${QT_MAJOR_VERSION}::CoreAddons
KF${QT_MAJOR_VERSION}::XmlGui
)

########### install files ###############
install(TARGETS kup-purger ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
