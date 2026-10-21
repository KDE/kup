// SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#pragma once

#include <QObject>
#include <QQmlEngine>

#include <KCompositeJob>

class RestoreJob : public KCompositeJob
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool wantsRestore READ wantsRestore WRITE setWantsRestore NOTIFY wantsRestoreChanged)

public:
    explicit RestoreJob(QObject *pParent = nullptr);

    Q_INVOKABLE void setBackup(QString pFsPath, QString pName, QString pRevision, QString pUsername);

    bool wantsRestore() const;
    void setWantsRestore(bool pWants);

    void start() override;

Q_SIGNALS:
    void wantsRestoreChanged();

private:
    void slotResult(KJob *pJob) override;

    bool mWantsRestore = false;
    QString mBackupFsPath;
    QString mBackupUsername;
    QString mBackupRevision;
    QString mBackupName;
};
