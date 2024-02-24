// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import QtQuick 2.0
import org.kde.plasma.plasmoid 2.0
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.plasma5support as Plasma5Support
import org.kde.kquickcontrolsaddons 2.0 as KQCAddons
import org.kde.kirigami 2.15 as Kirigami

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

	function action_configure() {
		KQCAddons.KCMShell.openSystemSettings("kcm_kup");
	}

	function action_reloadKup() {
		var service = backupPlans.serviceForSource("daemon");
		var operation = service.operationDescription("reload");
		service.startOperationCall(operation);
	}

	property int planCount: backupPlans.data["common"]["plan count"]

	fullRepresentation: FullRepresentation {}

	Component.onCompleted: {
		plasmoid.removeAction("configure");
		plasmoid.setAction("configure", i18nd("kup", "&Configure Kup..."), "configure");

		plasmoid.setAction("reloadKup", i18nd("kup", "&Reload backup plans"), "view-refresh");
	}
}
