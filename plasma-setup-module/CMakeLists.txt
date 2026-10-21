# SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
#
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

include(ECMQmlModule)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(selectbackup)
add_subdirectory(selectbackuputil)
add_subdirectory(auth)

