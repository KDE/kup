// SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include <QAbstractListModel>
#include <QDBusInterface>
#include <QObject>
#include <QtQmlIntegration/qqmlintegration.h>

#ifndef KUPAPPLET_PLUGIN_H
#define KUPAPPLET_PLUGIN_H

class PlanModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool shouldBeActive READ shouldBeActive NOTIFY shouldBeActiveChanged)
    Q_PROPERTY(bool anyBusy READ anyBusy NOTIFY anyBusyChanged)
    Q_PROPERTY(QString worstStatus READ worstStatus NOTIFY worstStatusChanged)
    Q_PROPERTY(QString activityState READ activityState NOTIFY activityStateChanged)
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
    QString worstStatus() const;
    QString activityState() const;
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
    void worstStatusChanged(QString);
    void activityStateChanged(QString);
    void anyDestsAvailableChanged(bool);

private:
    Q_SLOT void receiveUpdatedPlans(const QDBusMessage &msg);
    void setPlans(const QVariantList &plans);

    QVariantList mPlans;
    QDBusInterface mDBusIface;
};

#endif // KUPAPPLET_PLUGIN_H
