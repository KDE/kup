// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "planstatuswidget.h"
#include "backupplan.h"
#include "kupsettings.h"

#include <QBoxLayout>
#include <QLabel>
#include <QPushButton>

#include <KLocalizedString>

PlanStatusWidget::PlanStatusWidget(BackupPlan *pPlan, QWidget *pParent)
    : QGroupBox(pParent)
    , mPlan(pPlan)
{
    auto lVLayout1 = new QVBoxLayout;
    auto lVLayout2 = new QVBoxLayout;
    auto lHLayout1 = new QHBoxLayout;
    auto lHLayout2 = new QHBoxLayout;

    mDescriptionLabel = new QLabel(mPlan->mDescription);
    QFont lDescriptionFont = mDescriptionLabel->font();
    lDescriptionFont.setPointSizeF(lDescriptionFont.pointSizeF() + 2.0);
    lDescriptionFont.setBold(true);
    mDescriptionLabel->setFont(lDescriptionFont);
    mStatusIconLabel = new QLabel();
    // TODO: add dbus interface to be notified from daemon when this is updated.
    mStatusTextLabel = new QLabel(mPlan->statusText());
    auto lConfigureButton = new QPushButton(QIcon::fromTheme(QStringLiteral("configure")), xi18nc("@action:button", "Configure"));
    connect(lConfigureButton, SIGNAL(clicked()), this, SIGNAL(configureMe()));
    auto lRemoveButton = new QPushButton(QIcon::fromTheme(QStringLiteral("list-remove")), xi18nc("@action:button", "Remove"));
    connect(lRemoveButton, SIGNAL(clicked()), this, SIGNAL(removeMe()));
    auto lCopyButton = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-duplicate")), xi18nc("@action:button", "Duplicate"));
    connect(lCopyButton, &QPushButton::clicked, this, &PlanStatusWidget::duplicateMe);

    lVLayout2->addWidget(mDescriptionLabel);
    lVLayout2->addWidget(mStatusTextLabel);
    lHLayout1->addLayout(lVLayout2);
    lHLayout1->addStretch();
    lHLayout1->addWidget(mStatusIconLabel);
    lVLayout1->addLayout(lHLayout1);
    lHLayout2->addStretch();
    lHLayout2->addWidget(lCopyButton);
    lHLayout2->addWidget(lConfigureButton);
    lHLayout2->addWidget(lRemoveButton);
    lVLayout1->addLayout(lHLayout2);
    setLayout(lVLayout1);

    updateIcon();
}

void PlanStatusWidget::updateIcon()
{
    mStatusIconLabel->setPixmap(QIcon::fromTheme(BackupPlan::iconName(mPlan->backupStatus())).pixmap(64, 64));
}
