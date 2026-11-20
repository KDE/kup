// SPDX-FileCopyrightText: 2026 Tobias Fella <tobias.fella@kde.org>
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2

import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCMUtils
import org.kde.kup.kcm

KCMUtils.ScrollViewKCM {
    id: root

    header: Kirigami.Form {
        Kirigami.FormGroup {
            Kirigami.FormEntry {
                contentItem: QQC2.CheckBox {
                    text: i18nc("@label:checkbox", "Backups enabled")
                    checked: kcm.settings.backupsEnabled
                    onClicked: kcm.settings.backupsEnabled = !kcm.settings.backupsEnabled
                    KCMUtils.SettingStateBinding {
                        configObject: kcm.settings
                        target: parent
                        settingName: "BackupsEnabled"
                    }
                }
            }
        }
    }

    view: ListView {
        model: kcm.plans
        delegate: QQC2.ItemDelegate {
            id: delegate

            required property BackupPlan modelData
            required property int index

            enabled: kcm.settings.backupsEnabled
            width: ListView.view.width
            contentItem: Kirigami.IconTitleSubtitleWithActions {
                icon.name: delegate.modelData.statusIconName
                title: delegate.modelData.description
                subtitle: delegate.modelData.statusText
                actions: [
                    Kirigami.Action {
                        text: i18nc("@action:button", "Duplicate")
                        icon.name: "edit-copy"
                        onTriggered: kcm.duplicate(delegate.index)
                    },
                    Kirigami.Action {
                        text: i18nc("@action:button", "Configure")
                        icon.name: "configure"
                        onTriggered: ; //TODO
                    },
                    Kirigami.Action {
                        text: i18nc("@action:button", "Remove")
                        icon.name: "edit-delete-remove"
                        onTriggered: kcm.remove(delegate.index)
                    },
                ]
            }
        }
    }
}
