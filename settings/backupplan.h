// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef BACKUPPLAN_H
#define BACKUPPLAN_H

#include "../kupenums.h"

#include "backupplansettings.h"

using namespace Kup;

class Schedule;

class BackupPlan : public BackupPlanSettings
{
    Q_OBJECT
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    // TODO: emit statuChanged
    Q_PROPERTY(QString statusIconName READ statusIconName NOTIFY statusChanged)

public:
    BackupPlan(int pPlanNumber, KSharedConfigPtr pConfig, QObject *pParent = nullptr);
    int planNumber() const
    {
        return mPlanNumber;
    }
    virtual void setPlanNumber(int pPlanNumber);
    QString statusText() const;
    QString statusIconName() const;
    void copyFrom(const BackupPlan &pPlan);

    virtual QDateTime nextScheduledTime();
    virtual qint64 scheduleIntervalInSeconds() const;

    Status backupStatus() const;
    static QString iconName(Status pStatus);
    QString absoluteExcludesFilePath();

Q_SIGNALS:
    void statusChanged();

protected:
    void usrRead() override;
    int mPlanNumber;
};

#endif // BACKUPPLAN_H
