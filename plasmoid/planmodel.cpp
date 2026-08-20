// SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "planmodel.h"
#include "kupappletplugin_debug.h"

#include "../daemon/kupdaemon.h"

#include <QDBusMetaType>
#include <QDBusPendingReply>
#include <QDateTime>

#include <KFormat>
#include <KIO/OpenUrlJob>
#include <KNotificationJobUiDelegate>
#include <KWaylandExtras>
#include <KWindowSystem>

PlanModel::PlanModel(QObject *parent)
    : QAbstractListModel(parent)
    , mDaemon(KUP_DBUS_SERVICE_NAME, KUP_DBUS_OBJECT_PATH, QDBusConnection::sessionBus())
{
    qRegisterMetaType<QList<QVariantMap>>("QList<QVariantMap>");
    qDBusRegisterMetaType<QList<QVariantMap>>();

    auto *watcher = new QDBusServiceWatcher(KUP_DBUS_SERVICE_NAME, QDBusConnection::sessionBus(), QDBusServiceWatcher::WatchForOwnerChange, this);

    QObject::connect(watcher, &QDBusServiceWatcher::serviceRegistered, this, [this](const QString &serviceName) {
        Q_UNUSED(serviceName);
        fetchFromDaemon();
    });

    QObject::connect(&mDaemon, &KupDaemonIface::plansChanged, this, &PlanModel::setPlans);
    fetchFromDaemon();
}

void PlanModel::fetchFromDaemon()
{
    auto lPendingReply = mDaemon.getPlans();
    auto *lWatcher = new QDBusPendingCallWatcher(lPendingReply, this);
    QObject::connect(lWatcher, &QDBusPendingCallWatcher::finished, this, [this, lWatcher]() {
        lWatcher->deleteLater();
        QDBusPendingReply<QList<QVariantMap>> lPlansReply = *lWatcher;
        if (lPlansReply.isError()) {
            qCCritical(KUPAPPLETPLUGIN) << "could not get a reply from daemon to getPlans:" << lPlansReply.error().message();
        } else {
            setPlans(lPlansReply.value());
        }
    });
}

bool PlanModel::shouldBeActive() const
{
    for (int lRow = 0; lRow < rowCount(); lRow++) {
        QModelIndex lIndex = index(lRow, 0);
        if (data(lIndex, PlanModel::ScheduleTypeRole) == ScheduleType::MANUAL && data(lIndex, PlanModel::DestAvailableRole).toBool()) {
            return true;
        }
        if (data(lIndex, PlanModel::StatusRole) == Status::BAD) {
            return true;
        }
    }
    return false;
}

bool PlanModel::anyBusy() const
{
    for (int lRow = 0; lRow < rowCount(); lRow++) {
        QModelIndex lIndex = index(lRow, 0);
        if (data(lIndex, PlanModel::BusyRole).toBool()) {
            return true;
        }
    }
    return false;
}

Status PlanModel::worstStatus() const
{
    auto lWorst = Status::NO_STATUS;
    for (int lRow = 0; lRow < rowCount(); lRow++) {
        QModelIndex lIndex = index(lRow, 0);
        auto lStatus = static_cast<Status>(data(lIndex, PlanModel::StatusRole).toInt());
        if ((lWorst == Status::NO_STATUS) || (lWorst == Status::GOOD && (lStatus == Status::MEDIUM || lStatus == Status::BAD))
            || (lWorst == Status::MEDIUM && lStatus == Status::BAD)

        ) {
            lWorst = lStatus;
        }
    }

    return lWorst;
}

ExecutorState PlanModel::activityState() const
{
    for (int lRow = 0; lRow < rowCount(); lRow++) {
        QModelIndex lIndex = index(lRow, 0);
        if (data(lIndex, PlanModel::BusyRole).toBool()) {
            return static_cast<ExecutorState>(data(lIndex, PlanModel::ActivityStateRole).toInt());
        }
    }
    return ExecutorState::NOT_AVAILABLE;
}

bool PlanModel::anyDestsAvailable() const
{
    for (int lRow = 0; lRow < rowCount(); lRow++) {
        QModelIndex lIndex = index(lRow, 0);
        if (data(lIndex, PlanModel::DestAvailableRole).toBool()) {
            return true;
        }
    }
    return false;
}

void PlanModel::reloadConfig()
{
    mDaemon.reloadConfig();
}

void PlanModel::saveNewBackup(int pIdx)
{
    mDaemon.saveNewBackup(pIdx + 1);
}

void PlanModel::purgeBackups(int pIdx)
{
    if (KWindowSystem::isPlatformWayland()) {
        KWaylandExtras::xdgActivationToken(nullptr, {}).then(this, [this, pIdx](const QString &pToken) {
            mDaemon.purgeBackups(pIdx + 1, pToken);
        });
    } else {
        mDaemon.purgeBackups(pIdx + 1, QString());
    }
}

void PlanModel::browseBackup(int pIdx)
{
    if (KWindowSystem::isPlatformWayland()) {
        KWaylandExtras::xdgActivationToken(nullptr, {}).then(this, [this, pIdx](const QString &pToken) {
            mDaemon.browseBackup(pIdx + 1, pToken);
        });
    } else {
        mDaemon.browseBackup(pIdx + 1, QString());
    }
}

void PlanModel::openLogFile(int pIdx)
{
    QString lLogFilePath = data(index(pIdx, 0), LogFileRole).toString();
    auto *job = new KIO::OpenUrlJob(QUrl::fromLocalFile(lLogFilePath), QStringLiteral("text/x-log"));
    job->setUiDelegate(new KNotificationJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled));
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
    if (!pIndex.isValid() || pIndex.row() >= mPlans.count())
        return QVariant();

    const PlanInfo &lItem = mPlans.at(pIndex.row());

    switch (pRole) {
    case StatusRole:
        return lItem.status;
    case DescriptionRole:
        return lItem.description;
    case BusyRole:
        return lItem.busy;
    case ActivityStateRole:
        return lItem.activityState;
    case LogFileRole:
        return lItem.logFilePath;
    case LogFileExistsRole:
        return lItem.logFileExists;
    case TypeRole:
        return lItem.type;
    case ScheduleTypeRole:
        return lItem.scheduleType;
    case DestAvailableRole:
        return lItem.destAvailable;
    case LastCompleteBackupRole:
        return lItem.lastCompleteBackup;
    case LastBackupSizeRole:
        return lItem.lastBackupSize;
    case LastFreeSpaceRole:
        return lItem.lastFreeSpace;
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

void PlanModel::setPlans(const QList<QVariantMap> &pPlans)
{
    auto lPrevCount = rowCount();
    auto lPrevShouldBeActive = shouldBeActive();
    auto lPrevAnyBusy = anyBusy();
    auto lPrevWorstStatus = worstStatus();
    auto lPrevActivityState = activityState();
    auto lPrevAnyDestsAvailable = anyDestsAvailable();
    beginResetModel();
    mPlans.clear();
    for (const auto &lPlanInfoMap : pPlans) {
        PlanInfo lPlanInfo;
        lPlanInfo.status = static_cast<Status>(lPlanInfoMap["Status"].toInt());
        lPlanInfo.description = lPlanInfoMap["Description"].toString();
        lPlanInfo.busy = lPlanInfoMap["Busy"].toBool();
        lPlanInfo.activityState = static_cast<ExecutorState>(lPlanInfoMap["ActivityState"].toInt());
        lPlanInfo.logFilePath = lPlanInfoMap["LogFile"].toString();
        lPlanInfo.logFileExists = lPlanInfoMap["LogFileExists"].toBool();
        lPlanInfo.type = static_cast<BackupType>(lPlanInfoMap["Type"].toInt());
        lPlanInfo.scheduleType = static_cast<ScheduleType>(lPlanInfoMap["ScheduleType"].toInt());
        lPlanInfo.destAvailable = lPlanInfoMap["DestAvailable"].toBool();
        lPlanInfo.lastCompleteBackup = qdbus_cast<QDateTime>(lPlanInfoMap["LastCompleteBackup"]).toLocalTime();
        lPlanInfo.lastBackupSize = lPlanInfoMap["LastBackupSize"].toDouble();
        lPlanInfo.lastFreeSpace = lPlanInfoMap["LastFreeSpace"].toDouble();
        mPlans << lPlanInfo;
    }
    endResetModel();
    if (auto lNew = rowCount(); lPrevCount != lNew) {
        Q_EMIT countChanged(lNew);
    }
    if (auto lNew = shouldBeActive(); lPrevShouldBeActive != lNew) {
        Q_EMIT shouldBeActiveChanged(lNew);
    }
    if (auto lNew = anyBusy(); lPrevAnyBusy != lNew) {
        Q_EMIT anyBusyChanged(lNew);
    }
    if (auto lNew = worstStatus(); lPrevWorstStatus != lNew) {
        Q_EMIT worstStatusChanged(lNew);
    }
    if (auto lNew = activityState(); lPrevActivityState != lNew) {
        Q_EMIT activityStateChanged(lNew);
    }
    if (auto lNew = anyDestsAvailable(); lPrevAnyDestsAvailable != lNew) {
        Q_EMIT anyDestsAvailableChanged(lNew);
    }
}
