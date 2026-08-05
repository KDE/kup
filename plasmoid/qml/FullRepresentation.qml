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
					onClicked: planModel.reloadConfig()

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

		PlasmaComponents.ScrollView {
			anchors.fill: parent

			PlasmaComponents.ScrollBar.horizontal.policy: PlasmaComponents.ScrollBar.AlwaysOff

			contentWidth: availableWidth - plansList.leftMargin - plansList.rightMargin
			contentItem: ListView {
				id: plansList
				model: planModel
				clip: true
				currentIndex: -1

				spacing: Kirigami.Units.smallSpacing

				highlight: PlasmaExtras.Highlight {}
				highlightMoveDuration: Kirigami.Units.shortDuration
				highlightResizeDuration: Kirigami.Units.shortDuration

				delegate: PlanItem { }

				PlasmaExtras.PlaceholderMessage {
					visible: planModel.count === 0

					anchors.centerIn: parent
					width: parent.width - (Kirigami.Units.gridUnit * 4)

					text: i18nd("kup", "No backup plans configured")

					helpfulAction: QQC2.Action {
						text: i18nd("kup", "Configure Backup Plans…")
						icon.name: "configure"
						onTriggered: configureKup()
					}
				}
			}
		}
	}
}
