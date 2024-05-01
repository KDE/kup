// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import QtQuick
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasma5support as Plasma5Support
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCMUtils

PlasmoidItem {
	readonly property bool inPanel: (Plasmoid.location === PlasmaCore.Types.TopEdge
		|| Plasmoid.location === PlasmaCore.Types.RightEdge
		|| Plasmoid.location === PlasmaCore.Types.BottomEdge
		|| Plasmoid.location === PlasmaCore.Types.LeftEdge)

	function symbolicizeIconName(iconName) {
		const symbolicSuffix = "-symbolic";
		if (iconName.endsWith(symbolicSuffix)) {
			return iconName;
		}
		return iconName + symbolicSuffix;
	}

	switchWidth: Kirigami.Units.gridUnit * 10
	switchHeight: Kirigami.Units.gridUnit * 10
	toolTipMainText: getCommonStatus("tooltip title", "Error")
	toolTipSubText: getCommonStatus("tooltip subtitle", "No connection")

	Plasmoid.icon: inPanel
		? symbolicizeIconName( getCommonStatus("tooltip icon name", "kup"))
		: getCommonStatus("tooltip icon name", "kup")

	Plasmoid.status: getCommonStatus("tray icon active", false)
						  ? PlasmaCore.Types.ActiveStatus
						  : PlasmaCore.Types.PassiveStatus

	Plasma5Support.DataSource {
		id: backupPlans
		engine: "kup"
		connectedSources: sources

		onSourceAdded: function(source) {
			disconnectSource(source);
			connectSource(source);
		}
		onSourceRemoved: function(source) {
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

	fullRepresentation: FullRepresentation {}

	function reloadKup() {
		var service = backupPlans.serviceForSource("daemon");
		var operation = service.operationDescription("reload");
		service.startOperationCall(operation);
	}

	function configureKup() {
		KCMUtils.KCMLauncher.openSystemSettings("kcm_kup");
	}

	Plasmoid.contextualActions: [
		PlasmaCore.Action {
			text: i18nd("kup", "Reload Backup Plans")
			icon.name: "view-refresh"
			onTriggered: reloadKup()
		},
		PlasmaCore.Action {
			text: i18nd("kup", "Configure Backup Plans...")
			icon.name: "configure"
			onTriggered: configureKup()
		}
	]

	Component.onCompleted: {
		Plasmoid.removeInternalAction("configure");
	}
}
