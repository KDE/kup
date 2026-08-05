// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import QtQuick
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCMUtils

import plasma.applet.org.kde.kupapplet as Kup

PlasmoidItem {
	Kup.PlanModel {
		id: planModel
		Component.onCompleted: fetchFromDaemon()
	}

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

	function getActivityStateDescription(state) {
		return {
			"BACKUP_RUNNING": i18ndc("kup", "status in tooltip", "Saving backup"),
			"INTEGRITY_TESTING": i18ndc("kup", "status in tooltip", "Checking backup integrity"),
			"REPAIRING": i18ndc("kup", "status in tooltip", "Repairing backups"),
		}[state];
	}

	function getStatusDescription(status) {
		return {
			"GOOD": i18ndc("kup", "status in tooltip", "Backup status OK"),
			"MEDIUM": i18ndc("kup", "status in tooltip", "New backup suggested"),
			"BAD": i18ndc("kup", "status in tooltip", "New backup needed"),
			"NO_STATUS": "",
		}[status];
	}

	function getStatusIcon(status) {
		return {
			"GOOD": "security-high",
			"MEDIUM": "security-medium",
			"BAD": "security-low",
			"NO_STATUS": "preferences-system-backup",
		}[status];
	}

	switchWidth: Kirigami.Units.gridUnit * 10
	switchHeight: Kirigami.Units.gridUnit * 10
	toolTipMainText: {
		if (planModel.count === 0) {
			return i18ndc("kup", "status in tooltip", "No plans configured");
		} else if (planModel.anyBusy) {
			return getActivityStateDescription(planModel.activityState);
		} else if (planModel.worstStatus === "NO_STATUS") {
			return planModel.anyDestsAvailable
				? i18ndc("kup", "status in tooltip", "Backup destination available")
				: i18ndc("kup", "status in tooltip", "Backup destination not available");
		} else {
			return getStatusDescription(planModel.worstStatus);
		}
	}
	toolTipSubText: {
		if (planModel.count === 0) {
			return i18ndc("kup", "status in tooltip", "Open to set up backup plans");
		}
		return "";
	}

	Plasmoid.icon: inPanel
		? symbolicizeIconName(getStatusIcon(planModel.worstStatus))
		: getStatusIcon(planModel.worstStatus)

	Plasmoid.status: planModel.shouldBeActive
						  ? PlasmaCore.Types.ActiveStatus
						  : PlasmaCore.Types.PassiveStatus

	fullRepresentation: FullRepresentation {}

	function configureKup() {
		KCMUtils.KCMLauncher.openSystemSettings("kcm_kup");
	}

	Plasmoid.contextualActions: [
		PlasmaCore.Action {
			text: i18nd("kup", "Reload Backup Plans")
			icon.name: "view-refresh"
			priority: PlasmaCore.Action.HighPriority
			onTriggered: planModel.reloadConfig()
		},
		PlasmaCore.Action {
			text: i18nd("kup", "Configure Backup Plans…")
			icon.name: "configure"
			priority: PlasmaCore.Action.HighPriority
			onTriggered: configureKup()
		}
	]

	Component.onCompleted: {
		Plasmoid.removeInternalAction("configure");
	}
}
