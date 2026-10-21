// SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#pragma once

#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QObject>
#include <QQmlEngine>

struct HomeBackup {
    QString mFsPath;
    QString mBupName;
    // why both a datestamp and a date?
    // mainly to avoid re-formatting the date back to bup's expected format
    QString mDatestamp;
    QDateTime mDate;
    QString mUsername;

    bool operator==(const HomeBackup &) const = default;
};

class ExternalDriveBackupsModel : public QAbstractItemModel
{
    Q_OBJECT
    QML_ELEMENT

public:
    enum Roles {
        DriveDescriptionRole = Qt::UserRole + 1,
        DriveIsMountedRole,
        DriveMountPathRole,
        DriveUDIRole,
        DriveIsScanningRole,

        BackupUsernameRole,
        BackupDateRole,
        BackupFSPathRole,
        BackupRelativeFSPathRole,
        BackupNameRole,
        BackupRevisionRole,
    };
    Q_ENUM(Roles)

    explicit ExternalDriveBackupsModel(QObject *pParent = nullptr);
    ~ExternalDriveBackupsModel();

    int rowCount(const QModelIndex &pParent = QModelIndex()) const override;
    int columnCount(const QModelIndex &pParent = QModelIndex()) const override;
    QModelIndex index(int pRow, int pColumn, const QModelIndex &pParent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &pIndex) const override;
    bool hasChildren(const QModelIndex &pParent) const override;
    bool canFetchMore(const QModelIndex &pParent) const override;
    void fetchMore(const QModelIndex &pParent) override;
    QVariant data(const QModelIndex &pIndex, int pRole = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    void addDrive(const QString &pUdi);
    void removeDrive(const QString &pUdi);
    QList<QString> mDrives;
    QHash<QString, QList<HomeBackup>> mDriveBackups;
    QHash<QString, QPointer<QFutureWatcher<QString>>> mDriveSearchWatchers;
    void exploreFs(const QString &pPath);

    /* a bup repo looks like:
     * repo_root/
     *   backup_name/ (like "kup")
     *     tag/ (in the bup:// worker, a date is given instead of a tag name)
     *       home/
     *         username/
     */
    void listBupRepo(const QString &pDriveUdi, const QString &pRepoPath);
    void listBupRepoBackups(const QString &pDriveUdi, const QString &pRepoPath, const QString &pName);
    void listBupRepoDate(const QString &pDriveUdi, const QString &pRepoPath, const QString &pName, const QString &pDatestamp);
    void addHomeBackup(const QString &pDriveUdi, const QString &pRepoPath, const QString &pName, const QString &pDatestamp, const QString &pUsername);

    void driveMounted(const QString &pUdi, const QString &pMountPath);
};
