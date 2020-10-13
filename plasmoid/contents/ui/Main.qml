// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import QtQuick 2.0
import org.kde.plasma.plasmoid 2.0
import org.kde.plasma.core 2.0 as PlasmaCore

Item {
	Plasmoid.switchWidth: units.gridUnit * 10
	Plasmoid.switchHeight: units.gridUnit * 10
	Plasmoid.toolTipMainText: getCommonStatus("tooltip title", "Error")
	Plasmoid.toolTipSubText: getCommonStatus("tooltip subtitle", "No connection")
	Plasmoid.icon: getCommonStatus("tooltip icon name", "kup")
	Plasmoid.status: getCommonStatus("tray icon active", false)
						  ? PlasmaCore.Types.ActiveStatus
						  : PlasmaCore.Types.PassiveStatus

	PlasmaCore.DataSource {
		id: backupPlans
		engine: "backups"
		connectedSources: sources

		onSourceAdded: {
			disconnectSource(source);
			connectSource(source);
		}
		onSourceRemoved: {
			disconnectSource(source);
		}
	}


	function getCommonStatus(key, def){
		var result = backupPlans.data["common"][key];
		if(result === undefined) {
			result = def;
		}
		return result;
	}

	property int planCount: backupPlans.data["common"]["plan count"]

	Plasmoid.fullRepresentation: FullRepresentation {}
	Plasmoid.compactRepresentation: PlasmaCore.IconItem {
		source: "kup"
		width: units.iconSizes.medium;
		height: units.iconSizes.medium;

		MouseArea {
			anchors.fill: parent
			onClicked: plasmoid.expanded = !plasmoid.expanded
		}
	}
}
