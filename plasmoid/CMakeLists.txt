# SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
#
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

find_package(Plasma REQUIRED)

add_definitions(-DTRANSLATION_DOMAIN=\"kup\")

include_directories("../settings")

set(kupappletplugin_SRCS
    planmodel.cpp
    ../kupenums.h
)

ecm_qt_declare_logging_category(kupappletplugin_SRCS
    HEADER kupappletplugin_debug.h
    IDENTIFIER KUPAPPLETPLUGIN
    CATEGORY_NAME kup.applet.plugin
    EXPORT kup
    DESCRIPTION "Kup Applet Plugin"
)

set_source_files_properties(../daemon/org.kde.kupdaemon.KupDaemon.xml PROPERTIES
    CLASSNAME KupDaemonIface
    NO_NAMESPACE TRUE
)

qt_add_dbus_interface(kupappletplugin_SRCS ../daemon/org.kde.kupdaemon.KupDaemon.xml kupdaemoniface)

plasma_add_applet(org.kde.kupapplet
    QML_SOURCES
        qml/FullRepresentation.qml
        qml/PlanItem.qml
        qml/main.qml
    CPP_SOURCES
        ${kupappletplugin_SRCS}
    GENERATE_APPLET_CLASS
)

target_link_libraries(org.kde.kupapplet
    PRIVATE
    Qt::Core
    Qt::DBus
    Qt::Qml
    Plasma::Plasma
    KF6::KIOCore
    KF6::KIOFileWidgets
    KF6::WindowSystem
)
