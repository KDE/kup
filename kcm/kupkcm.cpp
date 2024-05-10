// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "kupkcm.h"
#include "backupplan.h"
#include "backupplanwidget.h"
#include "kupdaemon.h"
#include "kupsettings.h"
#include "planstatuswidget.h"

#include <QCheckBox>
#include <QDBusInterface>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedLayout>

#include <KAboutData>
#include <KConfigDialogManager>
#include <KLineEdit>
#include <KLocalizedString>
#include <KMessageBox>
#include <KPluginFactory>
#include <KProcess>

#if QT_VERSION_MAJOR == 5
#include <Kdelibs4ConfigMigrator>
#endif

K_PLUGIN_CLASS_WITH_JSON(KupKcm, "kcm_kup.json")

#if QT_VERSION_MAJOR == 5
KupKcm::KupKcm(QWidget *pParent, const QVariantList &pArgs)
    : KCModule(pParent, pArgs)
#else
KupKcm::KupKcm(QObject *pParent, const KPluginMetaData &md, const QVariantList &pArgs)
    : KCModule(pParent, md)
#endif
    , mSourcePageToShow(0)
{
#if QT_VERSION_MAJOR == 5
    KAboutData lAbout(QStringLiteral("kcm_kup"),
                      i18n("Kup Configuration Module"),
                      QStringLiteral("0.9.1"),
                      i18n("Configuration of backup plans for the Kup backup system"),
                      KAboutLicense::GPL,
                      i18n("Copyright (C) 2011-2020 Simon Persson"));
    lAbout.addAuthor(i18n("Simon Persson"), i18n("Maintainer"), "simon.persson@mykolab.com");
    lAbout.setTranslator(xi18nc("NAME OF TRANSLATORS", "Your names"), xi18nc("EMAIL OF TRANSLATORS", "Your emails"));
    setAboutData(new KAboutData(lAbout));
#endif
    setObjectName(QStringLiteral("kcm_kup")); // needed for the kconfigdialogmanager magic
    setButtons((Apply | buttons()) & ~Default);

    KProcess lBupProcess;
    lBupProcess << QStringLiteral("bup") << QStringLiteral("version");
    lBupProcess.setOutputChannelMode(KProcess::MergedChannels);
    int lExitCode = lBupProcess.execute();
    if (lExitCode >= 0) {
        mBupVersion = QString::fromUtf8(lBupProcess.readAllStandardOutput());
        KProcess lPar2Process;
        lPar2Process << QStringLiteral("bup") << QStringLiteral("fsck") << QStringLiteral("--par2-ok");
        mPar2Available = lPar2Process.execute() == 0;
    } else {
        mPar2Available = false;
    }

    KProcess lRsyncProcess;
    lRsyncProcess << QStringLiteral("rsync") << QStringLiteral("--version");
    lRsyncProcess.setOutputChannelMode(KProcess::MergedChannels);
    lExitCode = lRsyncProcess.execute();
    if (lExitCode >= 0) {
        QString lOutput = QString::fromLocal8Bit(lRsyncProcess.readLine());
        mRsyncVersion = lOutput.split(QLatin1Char(' '), Qt::SkipEmptyParts).at(2);
    }

    if (mBupVersion.isEmpty() && mRsyncVersion.isEmpty()) {
        auto lSorryIcon = new QLabel;
        lSorryIcon->setPixmap(QIcon::fromTheme(QStringLiteral("dialog-error")).pixmap(64, 64));
        QString lInstallMessage = i18n(
            "<h2>Backup programs are missing</h2>"
            "<p>Before you can activate any backup plan you need to "
            "install either of</p>"
            "<ul><li>bup, for versioned backups</li>"
            "<li>rsync, for synchronized backups</li></ul>");
        auto lSorryText = new QLabel(lInstallMessage);
        lSorryText->setWordWrap(true);
        auto lHLayout = new QHBoxLayout;
        lHLayout->addWidget(lSorryIcon);
        lHLayout->addWidget(lSorryText, 1);
#if QT_VERSION_MAJOR == 5
        setLayout(lHLayout);
#else
        widget()->setLayout(lHLayout);
#endif
    } else {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        Kdelibs4ConfigMigrator lMigrator(QStringLiteral("kup"));
        lMigrator.setConfigFiles(QStringList() << QStringLiteral("kuprc"));
        lMigrator.migrate();
#endif

        mConfig = KSharedConfig::openConfig(QStringLiteral("kuprc"));
        mSettings = new KupSettings(mConfig, this);
        for (int i = 0; i < mSettings->mNumberOfPlans; ++i) {
            mPlans.append(new BackupPlan(i + 1, mConfig, this));
            mConfigManagers.append(nullptr);
            mPlanWidgets.append(nullptr);
            mStatusWidgets.append(nullptr);
        }
        createSettingsFrontPage();
        addConfig(mSettings, mFrontPage);
        mStackedLayout = new QStackedLayout;
        mStackedLayout->addWidget(mFrontPage);
#if QT_VERSION_MAJOR == 5
        setLayout(mStackedLayout);
#else
        widget()->setLayout(mStackedLayout);
#endif
        QListIterator<QVariant> lIter(pArgs);
        while (lIter.hasNext()) {
            QVariant lVariant = lIter.next();
            if (lVariant.type() == QVariant::String) {
                QString lArgument = lVariant.toString();
                if (lArgument == QStringLiteral("show_sources") && lIter.hasNext()) {
                    mSourcePageToShow = lIter.next().toString().toInt();
                }
            }
        }
    }
}

#if QT_VERSION_MAJOR == 5
QSize KupKcm::sizeHint() const
{
    return {800, 600};
}
#endif

void KupKcm::load()
{
    if (mBupVersion.isEmpty() && mRsyncVersion.isEmpty()) {
        return;
    }
    // status will be set correctly after construction, set to checked here to
    // match the enabled status of other widgets
    mEnableCheckBox->setChecked(true);
    for (int i = 0; i < mSettings->mNumberOfPlans; ++i) {
        if (!mConfigManagers.at(i))
            createPlanWidgets(i);
        mConfigManagers.at(i)->updateWidgets();
    }
    for (int i = mSettings->mNumberOfPlans; i < mPlans.count();) {
        completelyRemovePlan(i);
    }
    KCModule::load();
    // this call is needed because it could have been set true before, now load() is called
    // because user pressed reset button. need to manually reset the "changed" state to false
    // in this case.
    unmanagedWidgetChangeState(false);
    if (mSourcePageToShow > 0) {
        mStackedLayout->setCurrentIndex(mSourcePageToShow);
        mPlanWidgets[mSourcePageToShow - 1]->showSourcePage();
        mSourcePageToShow = 0; // only trigger on first load after startup.
    }
}

void KupKcm::save()
{
    int lPlansRemoved = 0;
    for (int i = 0; i < mPlans.count(); ++i) {
        auto *lPlan = mPlans.at(i);
        auto *lManager = mConfigManagers.at(i);
        if (lManager == nullptr) {
            lPlan->setDefaults();
            lPlan->save();
            delete mPlans.takeAt(i);
            mConfigManagers.removeAt(i);
            mStatusWidgets.removeAt(i);
            delete mPlanWidgets.takeAt(i);
            ++lPlansRemoved;
            --i;
            continue;
        }
        if (lPlansRemoved != 0) {
            // config manager does not detect a changed group name of the config items.
            // To work around, read default settings - config manager will then notice
            // changed values and save current widget status into the config using the
            // new group name. If all settings for the plan already was default then
            // nothing was saved anyway, either under old or new group name.
            // This also removes all entries from the config file under the old plan
            // number group.
            lPlan->setDefaults();
            lPlan->save();

            lPlan->setPlanNumber(i + 1);
        }
        mPlanWidgets.at(i)->saveExtraData();
        lManager->updateSettings();
        mStatusWidgets.at(i)->updateIcon();
        if (lPlan->mDestinationType == 1 && lPlan->mExternalUUID.isEmpty()) {
#if QT_VERSION_MAJOR == 5
            QWidget *wid = this;
#else
            QWidget *wid = widget();
#endif
            KMessageBox::information(wid,
                                     xi18nc("@info %1 is the name of the backup plan",
                                            "%1 does not have a destination!<nl/>"
                                            "No backups will be saved by this plan.",
                                            lPlan->mDescription),
                                     xi18nc("@title:window", "Warning"),
                                     QString(),
                                     KMessageBox::Dangerous);
        }
    }
    mSettings->mNumberOfPlans = mPlans.count();
    mSettings->save();

    KCModule::save();

    QDBusInterface lInterface(KUP_DBUS_SERVICE_NAME, KUP_DBUS_OBJECT_PATH);
    if (lInterface.isValid()) {
        lInterface.call(QStringLiteral("reloadConfig"));
    } else {
        KProcess::startDetached(QStringLiteral("kup-daemon"));
    }
}

void KupKcm::updateChangedStatus()
{
    bool lHasUnmanagedChanged = false;
    foreach (KConfigDialogManager *lConfigManager, mConfigManagers) {
        if (!lConfigManager || lConfigManager->hasChanged()) {
            lHasUnmanagedChanged = true;
            break;
        }
    }
    if (mPlanWidgets.count() != mSettings->mNumberOfPlans)
        lHasUnmanagedChanged = true;
    unmanagedWidgetChangeState(lHasUnmanagedChanged);
}

void KupKcm::showFrontPage()
{
    mStackedLayout->setCurrentIndex(0);
}

void KupKcm::createSettingsFrontPage()
{
    mFrontPage = new QWidget;
    auto lHLayout = new QHBoxLayout;
    auto lVLayout = new QVBoxLayout;
    auto lScrollArea = new QScrollArea;
    auto lCentralWidget = new QWidget(lScrollArea);
    mVerticalLayout = new QVBoxLayout;
    lScrollArea->setWidget(lCentralWidget);
    lScrollArea->setWidgetResizable(true);
    lScrollArea->setFrameStyle(QFrame::NoFrame);

    auto lAddPlanButton = new QPushButton(QIcon::fromTheme(QStringLiteral("list-add")), xi18nc("@action:button", "Add New Plan"));
    connect(lAddPlanButton, &QPushButton::clicked, this, [this] {
        mPlans.append(new BackupPlan(mPlans.count() + 1, mConfig, this));
        if (mBupVersion.isEmpty())
            mPlans.last()->mBackupType = 1;
        mConfigManagers.append(nullptr);
        mPlanWidgets.append(nullptr);
        mStatusWidgets.append(nullptr);
        createPlanWidgets(mPlans.count() - 1);
        updateChangedStatus();
        emit mStatusWidgets.at(mPlans.count() - 1)->configureMe();
    });

    mEnableCheckBox = new QCheckBox(xi18nc("@option:check", "Backups Enabled"));
    mEnableCheckBox->setObjectName(QStringLiteral("kcfg_Backups enabled"));
    connect(mEnableCheckBox, &QCheckBox::toggled, lAddPlanButton, &QPushButton::setEnabled);

    lHLayout->addWidget(mEnableCheckBox);
    lHLayout->addStretch();
    lHLayout->addWidget(lAddPlanButton);
    lVLayout->addLayout(lHLayout);
    lVLayout->addWidget(lScrollArea);
    mFrontPage->setLayout(lVLayout);

    auto lFilediggerButton = new QPushButton(xi18nc("@action:button", "Open and restore from existing backups"));
    connect(lFilediggerButton, &QPushButton::clicked, [] {
        KProcess::startDetached(QStringLiteral("kup-filedigger"));
    });
    mVerticalLayout->addWidget(lFilediggerButton);
    mVerticalLayout->addStretch(1);
    lCentralWidget->setLayout(mVerticalLayout);
}

void KupKcm::createPlanWidgets(int pIndex)
{
    auto lPlanWidget = new BackupPlanWidget(mPlans.at(pIndex), mBupVersion, mRsyncVersion, mPar2Available);
    connect(lPlanWidget, SIGNAL(requestOverviewReturn()), this, SLOT(showFrontPage()));
    auto lConfigManager = new KConfigDialogManager(lPlanWidget, mPlans.at(pIndex));
    lConfigManager->setObjectName(objectName());
    connect(lConfigManager, SIGNAL(widgetModified()), this, SLOT(updateChangedStatus()));
    auto lStatusWidget = new PlanStatusWidget(mPlans.at(pIndex));
    connect(lStatusWidget, &PlanStatusWidget::removeMe, this, [this] {
        int lIndex = mStatusWidgets.indexOf(qobject_cast<PlanStatusWidget *>(sender()));
        if (lIndex < mSettings->mNumberOfPlans)
            partiallyRemovePlan(lIndex);
        else
            completelyRemovePlan(lIndex);
        updateChangedStatus();
    });
    connect(lStatusWidget, &PlanStatusWidget::configureMe, this, [this] {
        int lIndex = mStatusWidgets.indexOf(qobject_cast<PlanStatusWidget *>(sender()));
        mStackedLayout->setCurrentIndex(lIndex + 1);
    });
    connect(lStatusWidget, &PlanStatusWidget::duplicateMe, this, [this] {
        int lIndex = mStatusWidgets.indexOf(qobject_cast<PlanStatusWidget *>(sender()));
        auto lNewPlan = new BackupPlan(mPlans.count() + 1, mConfig, this);
        lNewPlan->copyFrom(*mPlans.at(lIndex));
        mPlans.append(lNewPlan);
        mConfigManagers.append(nullptr);
        mPlanWidgets.append(nullptr);
        mStatusWidgets.append(nullptr);
        createPlanWidgets(mPlans.count() - 1);
        // crazy trick to make the config system realize that stuff has changed
        // and will need to be saved.
        lNewPlan->setDefaults();
        updateChangedStatus();
    });
    connect(mEnableCheckBox, &QCheckBox::toggled, lStatusWidget, &PlanStatusWidget::setEnabled);
    connect(lPlanWidget->mDescriptionEdit, &KLineEdit::textChanged, lStatusWidget->mDescriptionLabel, &QLabel::setText);

    mConfigManagers[pIndex] = lConfigManager;
    mPlanWidgets[pIndex] = lPlanWidget;
    mStackedLayout->insertWidget(pIndex + 1, lPlanWidget);
    mStatusWidgets[pIndex] = lStatusWidget;
    // always insert at end, before the file digger button and strech space at the bottom.
    mVerticalLayout->insertWidget(mVerticalLayout->count() - 2, lStatusWidget);
}

void KupKcm::completelyRemovePlan(int pIndex)
{
    delete mConfigManagers.takeAt(pIndex);
    delete mStatusWidgets.takeAt(pIndex);
    delete mPlanWidgets.takeAt(pIndex);
    delete mPlans.takeAt(pIndex);
}

void KupKcm::partiallyRemovePlan(int pIndex)
{
    mConfigManagers.at(pIndex)->deleteLater();
    mConfigManagers[pIndex] = nullptr;
    mStatusWidgets.at(pIndex)->deleteLater();
    mStatusWidgets[pIndex] = nullptr;
}

#include "kupkcm.moc"
