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
    QDBusPendingCallWatcher *lWatcher = new QDBusPendingCallWatcher(lCall, this);
    QObject::connect(lWatcher, &QDBusPendingCallWatcher::finished, this, [this, lWatcher]() {
        QDBusPendingReply<QVariantList> lPlansReply = *lWatcher;
        if (lPlansReply.isError()) {
            qCDebug(KUPAPPLETPLUGIN) << "could not get a reply from daemon to getPlans:" << lPlansReply.error().message();
        } else {
            receiveUpdatedPlans(lPlansReply.reply());
        }
    });

    QDBusConnection::sessionBus().connect(mDBusIface.service(),
                                          mDBusIface.path(),
                                          mDBusIface.interface(),
                                          QStringLiteral("PlansChanged"),
                                          this,
                                          SLOT(receiveUpdatedPlans(QDBusMessage)));
}

bool PlanModel::shouldBeActive() const
{
    for (int lRow = 0; lRow < rowCount(); lRow++) {
        QModelIndex lIndex = index(lRow, 0);
        if (data(lIndex, PlanModel::ScheduleTypeRole) == QStringLiteral("MANUAL") && data(lIndex, PlanModel::DestAvailableRole) == true) {
            return true;
        }
        if (data(lIndex, PlanModel::StatusRole) == QStringLiteral("BAD")) {
            return true;
        }
    }
    return false;
}

bool PlanModel::anyBusy() const
{
    for (int lRow = 0; lRow < rowCount(); lRow++) {
        QModelIndex lIndex = index(lRow, 0);
        if (data(lIndex, PlanModel::BusyRole) == true) {
            return true;
        }
    }
    return false;
}

QString PlanModel::worstStatus() const
{
    QString lWorst = QStringLiteral("NO_STATUS");
    for (int lRow = 0; lRow < rowCount(); lRow++) {
        QModelIndex lIndex = index(lRow, 0);
        QString lStatus = data(lIndex, PlanModel::StatusRole).toString();
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
        QModelIndex lIndex = index(lRow, 0);
        if (data(lIndex, PlanModel::BusyRole) == true) {
            return data(lIndex, PlanModel::ActivityStateRole).toString();
        }
    }
    return QString();
}

bool PlanModel::anyDestsAvailable() const
{
    for (int lRow = 0; lRow < rowCount(); lRow++) {
        QModelIndex lIndex = index(lRow, 0);
        if (data(lIndex, PlanModel::DestAvailableRole) == true) {
            return true;
        }
    }
    return false;
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

void PlanModel::receiveUpdatedPlans(const QDBusMessage &pMsg)
{
    const auto lReceivedPlans = qdbus_cast<QVariantList>(pMsg.arguments().constFirst());
    QVariantList lPlans;
    for (const QVariant &lPlanVariant : lReceivedPlans) {
        lPlans << qdbus_cast<QVariantMap>(lPlanVariant);
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
