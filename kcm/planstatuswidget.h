// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef PLANSTATUSWIDGET_H
#define PLANSTATUSWIDGET_H

#include <QGroupBox>

class BackupPlan;
class KupSettings;

class QPushButton;

class QLabel;

class PlanStatusWidget : public QGroupBox
{
    Q_OBJECT
public:
    explicit PlanStatusWidget(BackupPlan *pPlan, QWidget *pParent = nullptr);
    BackupPlan *plan() const
    {
        return mPlan;
    }

    BackupPlan *mPlan;
    QLabel *mDescriptionLabel;
    QLabel *mStatusIconLabel;
    QLabel *mStatusTextLabel;

public slots:
    void updateIcon();

signals:
    void removeMe();
    void configureMe();
    void duplicateMe();
};

#endif // PLANSTATUSWIDGET_H
