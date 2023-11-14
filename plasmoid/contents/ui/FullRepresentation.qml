// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import QtQuick 2.2
import QtQuick.Layouts 1.1

import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 3.0 as PlasmaComponents
import org.kde.plasma.extras 2.0 as PlasmaExtras
import org.kde.kirigami 2.15 as Kirigami

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
					onClicked: plasmoid.action("reloadKup").trigger()

					PlasmaComponents.ToolTip {
							text: plasmoid.action("reloadKup").text
					}
			}

			PlasmaComponents.ToolButton {
					icon.name: "configure"
					onClicked: plasmoid.action("configure").trigger()

					PlasmaComponents.ToolTip {
							text: plasmoid.action("configure").text
					}
			}
		}
	}

	Item {
		anchors.fill: parent
		anchors.topMargin: Kirigami.Units.smallSpacing * 2
		focus: true

		PlasmaExtras.Heading {
			width: parent.width
			level: 3
			opacity: 0.6
			text: getCommonStatus("no plan reason", "")
			visible: planCount == 0
		}

		ColumnLayout {
			anchors.fill: parent

			PlasmaComponents.ScrollView {
				Layout.fillWidth: true
				Layout.fillHeight: true

				ListView {
					model: planCount
					delegate: planDelegate
					boundsBehavior: Flickable.StopAtBounds
				}
			}
		}

		Component {
			id: planDelegate

			Column {
				width: parent.width
				spacing: theme.defaultFont.pointSize
				RowLayout {
					width: parent.width
					Column {
						Layout.fillWidth: true
						PlasmaExtras.Heading {
							level: 3
							text: getPlanStatus(index, "description")
						}
						PlasmaExtras.Heading {
							level: 4
							text: getPlanStatus(index, "status heading")
						}
						PlasmaComponents.Label {
							text: getPlanStatus(index, "status details")
						}
					}
					Kirigami.Icon {
						source: getPlanStatus(index, "icon name")
						Layout.alignment: Qt.AlignRight | Qt.AlignTop
						Layout.preferredWidth: Kirigami.Units.iconSizes.huge
						Layout.preferredHeight: Kirigami.Units.iconSizes.huge
					}

				}
				Flow {
					width: parent.width
					spacing: theme.defaultFont.pointSize
					PlasmaComponents.Button {
						text: i18nd("kup", "Save new backup")
						visible: getPlanStatus(index, "destination available") &&
									!getPlanStatus(index, "busy")
						onClicked: startOperation(index, "save backup")
					}
					PlasmaComponents.Button {
						text: i18nd("kup", "Prune old backups")
						visible: getPlanStatus(index, "bup type") && getPlanStatus(index, "destination available")
						onClicked: startOperation(index, "remove backups")
					}
					PlasmaComponents.Button {
						text: i18nd("kup", "Show files")
						visible: getPlanStatus(index, "destination available")
						onClicked: startOperation(index, "show backup files")
					}
					PlasmaComponents.Button {
						text: i18nd("kup", "Show log file")
						visible: getPlanStatus(index, "log file exists")
						onClicked: startOperation(index, "show log file")
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
