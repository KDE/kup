// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2

import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

PlasmaComponents.Page {
	Layout.minimumWidth: Kirigami.Units.gridUnit * 12
	Layout.minimumHeight: Kirigami.Units.gridUnit * 12

	header: PlasmaExtras.PlasmoidHeading {
		visible: !(plasmoid.containmentDisplayHints &
		         PlasmaCore.Types.ContainmentDrawsPlasmoidHeading)

		RowLayout {
			anchors.fill: parent

			Item {
				Layout.fillWidth: true
			}

			PlasmaComponents.ToolButton {
					icon.name: "view-refresh"
					onClicked: reloadKup()

					PlasmaComponents.ToolTip {
						text: i18nd("kup", "Reload Backup Plans")
					}
			}

			PlasmaComponents.ToolButton {
					icon.name: "configure"
					onClicked: configureKup()

					PlasmaComponents.ToolTip {
						text: i18nd("kup", "Configure Backup Plans")
					}
			}
		}
	}

	Item {
		anchors.fill: parent
		anchors.topMargin: Kirigami.Units.smallSpacing * 2
		focus: true

		Kirigami.Heading {
			width: parent.width
			level: 3
			opacity: 0.6
			text: getCommonStatus("no plan reason", "")
			visible: planCount == 0
		}

		PlasmaComponents.ScrollView {
			anchors.fill: parent

			PlasmaComponents.ScrollBar.horizontal.policy: PlasmaComponents.ScrollBar.AlwaysOff

			contentWidth: availableWidth - plansList.leftMargin - plansList.rightMargin
			contentItem: ListView {
				id: plansList
				model: planCount
				clip: true
				currentIndex: -1

				spacing: Kirigami.Units.smallSpacing

				highlight: PlasmaExtras.Highlight {}
				highlightMoveDuration: Kirigami.Units.shortDuration
				highlightResizeDuration: Kirigami.Units.shortDuration

				delegate: PlanItem {}

				PlasmaExtras.PlaceholderMessage {
					visible: planCount == 0

					anchors.centerIn: parent
					width: parent.width - (Kirigami.Units.gridUnit * 4)

					text: getCommonStatus("no plan reason", "")

					helpfulAction: QQC2.Action {
						text: i18nd("kup", "Configure Backup Plans…")
						icon.name: "configure"
						onTriggered: configureKup()
					}
				}
			}
		}
	}

	function getPlanStatus(planNumber, key){
		return backupPlans.data["plan " + planNumber.toString()][key];
	}

	function startOperation(i, name) {
		var service = backupPlans.serviceForSource(i.toString());
		var operation = service.operationDescription(name);
		service.startOperationCall(operation);
	}
}
