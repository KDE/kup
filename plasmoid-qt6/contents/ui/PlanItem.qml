// SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import QtQuick
import QtQuick.Controls as QQC2

import org.kde.plasma.extras as PlasmaExtras
import org.kde.plasma.plasmoid

PlasmaExtras.ExpandableListItem {
    required index

    title: getPlanStatus(index, "description")
    subtitle: {
        // heading is nonempty when some action is being performed
        const heading = getPlanStatus(index, "status heading");
        const details = getPlanStatus(index, "status details");
        return (heading !== "") ? heading : details;
    }
    subtitleCanWrap: true
    icon: {
        // icon name is empty for manual backups
        const iconName = getPlanStatus(index, "icon name");
        return (iconName !== "") ? iconName : "preferences-system-backup";
    }
    // TODO add an emblem when we have a better emblem icon in Breeze to represent syncing/working

    QQC2.Action {
        id: openFilesAction
        enabled: getPlanStatus(index, "destination available")
        icon.name: "document-open-folder-symbolic"
        text: i18nd("kup", "Show Files")
        onTriggered: startOperation(index, "show backup files")
    }

    QQC2.Action {
        id: saveNewAction
        enabled: getPlanStatus(index, "destination available") && !getPlanStatus(index, "busy")
        icon.name: "document-save"
        text: i18nd("kup", "Save Backup")
        onTriggered: startOperation(index, "save backup")
    }

    QQC2.Action {
        id: pruneAction
        enabled: getPlanStatus(index, "bup type") && getPlanStatus(index, "destination available")
        icon.name: "edit-clear-history"
        text: i18nd("kup", "Prune Old Backups")
        onTriggered: startOperation(index, "remove backups")
    }

    QQC2.Action {
        id: openLogAction
        enabled: getPlanStatus(index, "log file exists")
        icon.name: "folder-log-symbolic"
        text: i18nd("kup", "View Log")
        onTriggered: startOperation(index, "show log file")
    }

    defaultActionButtonAction: openFilesAction.enabled ? openFilesAction : null
    contextualActions: [saveNewAction, pruneAction, openLogAction]
}
