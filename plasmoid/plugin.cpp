// SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "plugin.h"
#include "kupappletplugin_debug.h"

#include "../daemon/kupdaemon.h"

#include <QDBusPendingReply>
#include <QDateTime>

#include <KFormat>
#include <KIO/OpenUrlJob>

PlanModel::PlanModel(QObject *parent)
    : QAbstractListModel(parent)
    , mDBusIface(KUP_DBUS_SERVICE_NAME, KUP_DBUS_OBJECT_PATH)
{
}

void PlanModel::fetchFromDaemon()
{
    if (!mDBusIface.isValid()) {
        qCCritical(KUPAPPLETPLUGIN) << "could not initialize interface to kup daemon:" << mDBusIface.lastError();
        return;
    }
    QDBusPendingCall lCall = mDBusIface.asyncCall(QStringLiteral("getPlans"));
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(lCall, this);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher]() {
        QDBusPendingReply<QVariantList> lPlansReply = *watcher;
        if (lPlansReply.isError()) {
            qCDebug(KUPAPPLETPLUGIN) << "could not get a reply from daemon to getPlans:" << lPlansReply.error().message();
        } else {
            const QVariantList lArgs = qdbus_cast<QVariantList>(lPlansReply.value());
            QVariantList lPlans;
            for (const QVariant &lArg : lArgs) {
                lPlans << qdbus_cast<QVariantMap>(lArg);
            }
            setPlans(lPlans);
        }
    });

    QDBusConnection::sessionBus()
        .connect(mDBusIface.service(), mDBusIface.path(), mDBusIface.interface(), QStringLiteral("PlansChanged"), this, SLOT(slotPlansChanged(QDBusMessage)));
}

void PlanModel::slotPlansChanged(const QDBusMessage &pMsg)
{
    const QVariantList lArgs = qdbus_cast<QVariantList>(pMsg.arguments().first());
    QVariantList lPlans;
    for (const QVariant &lArg : lArgs) {
        lPlans << qdbus_cast<QVariantMap>(lArg);
    }
    setPlans(lPlans);
}

void PlanModel::setPlans(const QVariantList &pPlans)
{
    beginResetModel();
    mPlans = pPlans;
    endResetModel();
    Q_EMIT countChanged(rowCount());
    Q_EMIT shouldBeActiveChanged(shouldBeActive());
    Q_EMIT anyBusyChanged(anyBusy());
    Q_EMIT worstStatusChanged(worstStatus());
    Q_EMIT activityStateChanged(activityState());
    Q_EMIT anyDestsAvailableChanged(anyDestsAvailable());
}

int PlanModel::rowCount(const QModelIndex &pParent) const
{
    if (pParent.isValid()) {
        return 0;
    }
    return mPlans.count();
}

QVariant PlanModel::data(const QModelIndex &pIndex, int pRole) const
{
    if (!pIndex.isValid() || pIndex.row() < 0 || pIndex.row() >= mPlans.count())
        return QVariant();

    const QVariantMap &lItem = mPlans.at(pIndex.row()).toMap();

    switch (pRole) {
    case StatusRole:
        return lItem["Status"];
    case DescriptionRole:
        return lItem["Description"];
    case BusyRole:
        return lItem["Busy"];
    case ActivityStateRole:
        return lItem["ActivityState"];
    case LogFileRole:
        return lItem["LogFile"];
    case LogFileExistsRole:
        return lItem["LogFileExists"];
    case TypeRole:
        return lItem["Type"];
    case ScheduleTypeRole:
        return lItem["ScheduleType"];
    case DestAvailableRole:
        return lItem["DestAvailable"];
    case LastCompleteBackupRole:
        return KFormat().formatRelativeDate(qdbus_cast<QDateTime>(lItem["LastCompleteBackup"]).toLocalTime().date(), QLocale::LongFormat);
    case LastBackupSizeRole:
        return lItem["LastBackupSize"];
    case LastFreeSpaceRole:
        return lItem["LastFreeSpace"];
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> PlanModel::roleNames() const
{
    QHash<int, QByteArray> lRoles;
    lRoles[StatusRole] = "status";
    lRoles[DescriptionRole] = "description";
    lRoles[BusyRole] = "busy";
    lRoles[ActivityStateRole] = "activityState";
    lRoles[LogFileRole] = "logFile";
    lRoles[LogFileExistsRole] = "logFileExists";
    lRoles[TypeRole] = "type";
    lRoles[ScheduleTypeRole] = "scheduleType";
    lRoles[DestAvailableRole] = "destAvailable";
    lRoles[LastCompleteBackupRole] = "lastCompleteBackup";
    lRoles[LastBackupSizeRole] = "lastBackupSize";
    lRoles[LastFreeSpaceRole] = "lastFreeSpace";
    return lRoles;
}

bool PlanModel::shouldBeActive() const
{
    for (int lRow = 0; lRow < rowCount(); lRow++) {
        QModelIndex lIdx = index(lRow, 0);
        if (data(lIdx, PlanModel::ScheduleTypeRole) == QStringLiteral("MANUAL") && data(lIdx, PlanModel::DestAvailableRole) == true) {
            return true;
        }
        if (data(lIdx, PlanModel::StatusRole) == QStringLiteral("BAD")) {
            return true;
        }
    }
    return false;
}

bool PlanModel::anyBusy() const
{
    for (int lRow = 0; lRow < rowCount(); lRow++) {
        QModelIndex idx = index(lRow, 0);
        if (data(idx, PlanModel::BusyRole) == true) {
            return true;
        }
    }
    return false;
}

bool PlanModel::anyDestsAvailable() const
{
    for (int lRow = 0; lRow < rowCount(); lRow++) {
        QModelIndex lIdx = index(lRow, 0);
        if (data(lIdx, PlanModel::DestAvailableRole) == true) {
            return true;
        }
    }
    return false;
}

QString PlanModel::worstStatus() const
{
    QString lWorst = QStringLiteral("NO_STATUS");
    for (int lRow = 0; lRow < rowCount(); lRow++) {
        QModelIndex lIdx = index(lRow, 0);
        QString lStatus = data(lIdx, PlanModel::StatusRole).toString();
        if ((lWorst == QStringLiteral("NO_STATUS"))
            || (lWorst == QStringLiteral("GOOD") && (lStatus == QStringLiteral("MEDIUM") || lStatus == QStringLiteral("BAD")))
            || (lWorst == QStringLiteral("MEDIUM") && lStatus == QStringLiteral("BAD"))

        ) {
            lWorst = lStatus;
        }
    }

    return lWorst;
}

QString PlanModel::activityState() const
{
    for (int lRow = 0; lRow < rowCount(); lRow++) {
        QModelIndex lIdx = index(lRow, 0);
        if (data(lIdx, PlanModel::BusyRole) == true) {
            return data(lIdx, PlanModel::ActivityStateRole).toString();
        }
    }
    return QString();
}

void PlanModel::reloadConfig()
{
    mDBusIface.asyncCall(QStringLiteral("reloadConfig"));
}

void PlanModel::saveNewBackup(int pIdx)
{
    mDBusIface.asyncCall(QStringLiteral("saveNewBackup"), pIdx);
}

void PlanModel::purgeBackups(int pIdx)
{
    mDBusIface.asyncCall(QStringLiteral("purgeBackups"), pIdx);
}

void PlanModel::browseBackup(int pIdx)
{
    mDBusIface.asyncCall(QStringLiteral("browseBackup"), pIdx);
}

void PlanModel::openLogFile(int pIdx)
{
    QString lLogFilePath = data(index(pIdx, 0), LogFileRole).toString();
    auto *job = new KIO::OpenUrlJob(QUrl::fromLocalFile(lLogFilePath), QStringLiteral("text/x-log"));
    job->start();
}
