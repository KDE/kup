// SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef KUPAPPLET_PLANMODEL_H
#define KUPAPPLET_PLANMODEL_H

#include "../kupenums.h"

#include "kupdaemoniface.h"

#include <QAbstractListModel>
#include <QDBusInterface>
#include <QObject>
#include <QtQmlIntegration/qqmlintegration.h>

using namespace Kup;

struct PlanInfo {
    Status status;
    QString description;
    bool busy;
    ExecutorState activityState;
    bool logFileExists;
    QString logFilePath;
    BackupType type;
    ScheduleType scheduleType;
    bool destAvailable;
    QDateTime lastCompleteBackup;
    double lastBackupSize;
    double lastFreeSpace;
};

class PlanModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool shouldBeActive READ shouldBeActive NOTIFY shouldBeActiveChanged)
    Q_PROPERTY(bool anyBusy READ anyBusy NOTIFY anyBusyChanged)
    Q_PROPERTY(Status worstStatus READ worstStatus NOTIFY worstStatusChanged)
    Q_PROPERTY(ExecutorState activityState READ activityState NOTIFY activityStateChanged)
    Q_PROPERTY(bool anyDestsAvailable READ anyDestsAvailable NOTIFY anyDestsAvailableChanged)

    QML_ELEMENT

public:
    enum PlanRoles {
        StatusRole = Qt::UserRole + 1,
        DescriptionRole,
        BusyRole,
        ActivityStateRole,
        LogFileRole,
        LogFileExistsRole,
        TypeRole,
        ScheduleTypeRole,
        DestAvailableRole,
        LastCompleteBackupRole,
        LastBackupSizeRole,
        LastFreeSpaceRole,
    };
    Q_ENUM(PlanRoles)

    explicit PlanModel(QObject *parent = nullptr);
    Q_INVOKABLE void fetchFromDaemon();

    bool shouldBeActive() const;
    bool anyBusy() const;
    Status worstStatus() const;
    ExecutorState activityState() const;
    bool anyDestsAvailable() const;

    Q_INVOKABLE void reloadConfig();
    Q_INVOKABLE void saveNewBackup(int pIdx);
    Q_INVOKABLE void purgeBackups(int pIdx);
    Q_INVOKABLE void browseBackup(int pIdx);
    Q_INVOKABLE void openLogFile(int pIdx);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

Q_SIGNALS:
    void countChanged(int);
    void shouldBeActiveChanged(bool);
    void anyBusyChanged(bool);
    void worstStatusChanged(Status);
    void activityStateChanged(ExecutorState);
    void anyDestsAvailableChanged(bool);

private:
    void setPlans(const QList<QVariantMap> &plans);

    QList<PlanInfo> mPlans;
    KupDaemonIface mDaemon;
};

#endif // KUPAPPLET_PLANMODEL_H
