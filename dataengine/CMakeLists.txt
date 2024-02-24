# SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

#this is a library so it needs to enforce it's translation domain, not use the application's domain.
add_definitions(-DTRANSLATION_DOMAIN="kup")
include_directories("../daemon")

# Plasma Data Engine
set(plasma_engine_kup_SRCS
	kupengine.cpp
	kupservice.cpp
	kupjob.cpp
	)
add_library(plasma_engine_kup MODULE ${plasma_engine_kup_SRCS})
target_link_libraries(plasma_engine_kup
	Qt::Network
)

if(QT_MAJOR_VERSION STREQUAL "6")
	target_link_libraries(plasma_engine_kup Plasma::Plasma5Support Plasma::Plasma)
	install(TARGETS plasma_engine_kup DESTINATION ${PLASMA5SUPPORT_DATAENGINES_PLUGINDIR})
	install(FILES kupservice.operations kupdaemonservice.operations DESTINATION ${PLASMA5SUPPORT_DATA_INSTALL_DIR}/services)
else()
	target_link_libraries(plasma_engine_kup KF5::Plasma)
	install(TARGETS plasma_engine_kup DESTINATION ${KDE_INSTALL_PLUGINDIR}/plasma/dataengine)
	install(FILES kupservice.operations kupdaemonservice.operations DESTINATION ${PLASMA_DATA_INSTALL_DIR}/services)
endif()
