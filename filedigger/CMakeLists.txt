# SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

include_directories("../daemon")
include_directories("../kcm")
include_directories("../kioworker")
include_directories("../settings")

set(filedigger_SRCS
filedigger.cpp
main.cpp
mergedvfs.cpp
mergedvfsmodel.cpp
restoredialog.cpp
restorejob.cpp
versionlistdelegate.cpp
versionlistmodel.cpp
../kioworker/vfshelpers.cpp
../kcm/dirselector.cpp
../settings/kuputils.cpp
)

ecm_qt_declare_logging_category(filedigger_SRCS
    HEADER kupfiledigger_debug.h
    IDENTIFIER KUPFILEDIGGER
    CATEGORY_NAME kup.filedigger
    DEFAULT_SEVERITY Warning
    EXPORT kup
    DESCRIPTION "Kup Filedigger"
)

add_definitions(-fexceptions)

ki18n_wrap_ui(filedigger_SRCS restoredialog.ui)
add_executable(kup-filedigger ${filedigger_SRCS})
target_link_libraries(kup-filedigger
Qt::Core
Qt::Gui
KF${QT_MAJOR_VERSION}::KIOCore
KF${QT_MAJOR_VERSION}::KIOFileWidgets
KF${QT_MAJOR_VERSION}::I18n
KF${QT_MAJOR_VERSION}::JobWidgets
KF${QT_MAJOR_VERSION}::WidgetsAddons
KF${QT_MAJOR_VERSION}::XmlGui
LibGit2::LibGit2
)

########### install files ###############
install(TARGETS kup-filedigger ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
