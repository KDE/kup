// SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import QtQuick
import QtQuick.Controls as QQC2

import org.kde.plasma.extras as PlasmaExtras
import org.kde.plasma.plasmoid

PlasmaExtras.ExpandableListItem {
    required index

    required property string status
    required property string description
    required property bool busy
    required property string activityState
    required property string logFile
    required property bool logFileExists
    required property string type
    required property string scheduleType
    required property bool destAvailable
    required property string lastCompleteBackup
    required property real lastBackupSize
    required property real lastFreeSpace

    title: description
    subtitle: {
        const line1 = busy
        ? getActivityStateDescription(activityState)
        : getStatusDescription(status);

        return line1 + (line1.length > 0 ? "\n" : "") + i18ndc("kup", "%1 is formatted relative date", "Last saved: %1", lastCompleteBackup);
    }
    subtitleCanWrap: true
    icon: getStatusIcon(status);

    // TODO add an emblem when we have a better emblem icon in Breeze to represent syncing/working

    QQC2.Action {
        id: openFilesAction
        enabled: destAvailable
        icon.name: "document-open-folder-symbolic"
        text: i18nd("kup", "Show Files")
        onTriggered: planModel.browseBackup(index)
    }

    QQC2.Action {
        id: saveNewAction
        enabled: destAvailable && !busy
        icon.name: "document-save"
        text: i18nd("kup", "Save Backup")
        onTriggered: planModel.saveNewBackup(index)
    }

    QQC2.Action {
        id: pruneAction
        enabled: type === "BupType" && destAvailable
        icon.name: "edit-clear-history"
        text: i18nd("kup", "Prune Old Backups")
        onTriggered: planModel.purgeBackups(index)
    }

    QQC2.Action {
        id: openLogAction
        enabled: logFileExists
        icon.name: "folder-log-symbolic"
        text: i18nd("kup", "View Log")
        onTriggered: planModel.openLogFile(index)
    }

    defaultActionButtonAction: openFilesAction.enabled ? openFilesAction : null
    contextualActions: [saveNewAction, pruneAction, openLogAction]
}
