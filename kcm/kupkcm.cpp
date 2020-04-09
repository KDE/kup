// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL

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
#include <Kdelibs4ConfigMigrator>
#include <KLineEdit>
#include <KLocalizedString>
#include <KMessageBox>
#include <KPluginFactory>
#include <KProcess>

K_PLUGIN_FACTORY(KupKcmFactory, registerPlugin<KupKcm>();)

KupKcm::KupKcm(QWidget *pParent, const QVariantList &pArgs)
    : KCModule(pParent, pArgs)
{
	KAboutData lAbout(QStringLiteral("kcm_kup"), i18n("Kup Configuration Module"),
	                  QStringLiteral("0.7.3"),
	                  i18n("Configuration of backup plans for the Kup backup system"),
	                  KAboutLicense::GPL, i18n("Copyright (C) 2011-2020 Simon Persson"));
	lAbout.addAuthor(i18n("Simon Persson"), i18n("Maintainer"), "simon.persson@mykolab.com");
	lAbout.setTranslator(xi18nc("NAME OF TRANSLATORS", "Your names"),
	                     xi18nc("EMAIL OF TRANSLATORS", "Your emails"));
	setAboutData(new KAboutData(lAbout));
	setObjectName(QStringLiteral("kcm_kup")); //needed for the kconfigdialogmanager magic
	setButtons((Apply | buttons()) & ~Default);

	KProcess lBupProcess;
	lBupProcess << QStringLiteral("bup") << QStringLiteral("version");
	lBupProcess.setOutputChannelMode(KProcess::MergedChannels);
	int lExitCode = lBupProcess.execute();
	if(lExitCode >= 0) {
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
	if(lExitCode >= 0) {
		QString lOutput = QString::fromLocal8Bit(lRsyncProcess.readLine());
		mRsyncVersion = lOutput.split(QLatin1Char(' '), QString::SkipEmptyParts).at(2);
	}

	if(mBupVersion.isEmpty() && mRsyncVersion.isEmpty()) {
		auto lSorryIcon = new QLabel;
		lSorryIcon->setPixmap(QIcon::fromTheme(QStringLiteral("dialog-error")).pixmap(64, 64));
		QString lInstallMessage = i18n("<h2>Backup programs are missing</h2>"
		                               "<p>Before you can activate any backup plan you need to "
		                               "install either of</p>"
		                               "<ul><li>bup, for versioned backups</li>"
		                               "<li>rsync, for synchronized backups</li></ul>");
		auto lSorryText = new QLabel(lInstallMessage);
		lSorryText->setWordWrap(true);
		auto lHLayout = new QHBoxLayout;
		lHLayout->addWidget(lSorryIcon);
		lHLayout->addWidget(lSorryText, 1);
		setLayout(lHLayout);
	} else {
		Kdelibs4ConfigMigrator lMigrator(QStringLiteral("kup"));
		lMigrator.setConfigFiles(QStringList() << QStringLiteral("kuprc"));
		lMigrator.migrate();

		mConfig = KSharedConfig::openConfig(QStringLiteral("kuprc"));
		mSettings = new KupSettings(mConfig, this);
		for(int i = 0; i < mSettings->mNumberOfPlans; ++i) {
			mPlans.append(new BackupPlan(i+1, mConfig, this));
			mConfigManagers.append(nullptr);
			mPlanWidgets.append(nullptr);
			mStatusWidgets.append(nullptr);
		}
		createSettingsFrontPage();
		addConfig(mSettings, mFrontPage);
		mStackedLayout = new QStackedLayout;
		mStackedLayout->addWidget(mFrontPage);
		setLayout(mStackedLayout);
	}
}

QSize KupKcm::sizeHint() const {
	int lBaseWidth = fontMetrics().horizontalAdvance('M');
	return {lBaseWidth * 65, lBaseWidth * 35};
}

void KupKcm::load() {
	if(mBupVersion.isEmpty() && mRsyncVersion.isEmpty()) {
		return;
	}
	// status will be set correctly after construction, set to checked here to
	// match the enabled status of other widgets
	mEnableCheckBox->setChecked(true);
	for(int i = 0; i < mSettings->mNumberOfPlans; ++i) {
		if(!mConfigManagers.at(i))
			createPlanWidgets(i);
		mConfigManagers.at(i)->updateWidgets();
	}
	for(int i = mSettings->mNumberOfPlans; i < mPlans.count();) {
		completelyRemovePlan(i);
	}
	KCModule::load();
	// this call is needed because it could have been set true before, now load() is called
	// because user pressed reset button. need to manually reset the "changed" state to false
	// in this case.
	unmanagedWidgetChangeState(false);
}

void KupKcm::save() {
	KConfigDialogManager *lManager;
	BackupPlan *lPlan;
	int lPlansRemoved = 0;
	for(int i=0; i < mPlans.count(); ++i) {
		lPlan = mPlans.at(i);
		lManager = mConfigManagers.at(i);
		if(lManager != nullptr) {
			if(lPlansRemoved != 0) {
				lPlan->removePlanFromConfig();
				lPlan->setPlanNumber(i + 1);
				// config manager does not detect a changed group name of the config items.
				// To work around, read default settings - config manager will then notice
				// changed values and save current widget status into the config using the
				// new group name. If all settings for the plan already was default then
				// nothing was saved anyway, either under old or new group name.
				lPlan->setDefaults();
			}
			mPlanWidgets.at(i)->saveExtraData();
			lManager->updateSettings();
			mStatusWidgets.at(i)->updateIcon();
			if(lPlan->mDestinationType == 1 && lPlan->mExternalUUID.isEmpty()) {
				KMessageBox::information(this, xi18nc("@title:window", "Warning"),
				                         xi18nc("@info %1 is the name of the backup plan",
				                                "%1 does not have a destination!<nl/>"
				                                "No backups will be saved by this plan.",
				                                lPlan->mDescription),
				                         QString(), KMessageBox::Dangerous);
			}
		}
		else {
			lPlan->removePlanFromConfig();
			delete mPlans.takeAt(i);
			mConfigManagers.removeAt(i);
			mStatusWidgets.removeAt(i);
			mPlanWidgets.removeAt(i);
			++lPlansRemoved;
			--i;
		}
	}
	mSettings->mNumberOfPlans = mPlans.count();
	mSettings->save();

	KCModule::save();

	QDBusInterface lInterface(KUP_DBUS_SERVICE_NAME, KUP_DBUS_OBJECT_PATH);
	if(lInterface.isValid()) {
		lInterface.call(QStringLiteral("reloadConfig"));
	} else {
		KProcess::startDetached(QStringLiteral("kup-daemon"));
	}
}

void KupKcm::updateChangedStatus() {
	bool lHasUnmanagedChanged = false;
	foreach(KConfigDialogManager *lConfigManager, mConfigManagers) {
		if(!lConfigManager || lConfigManager->hasChanged()) {
			lHasUnmanagedChanged = true;
			break;
		}
	}
	if(mPlanWidgets.count() != mSettings->mNumberOfPlans)
		lHasUnmanagedChanged = true;
	unmanagedWidgetChangeState(lHasUnmanagedChanged);
}

void KupKcm::showFrontPage() {
	mStackedLayout->setCurrentIndex(0);
}

void KupKcm::createSettingsFrontPage() {
	mFrontPage = new QWidget;
	auto lHLayout = new QHBoxLayout;
	auto lVLayout = new QVBoxLayout;
	auto lScrollArea = new QScrollArea;
	auto lCentralWidget = new QWidget(lScrollArea);
	mVerticalLayout = new QVBoxLayout;
	lScrollArea->setWidget(lCentralWidget);
	lScrollArea->setWidgetResizable(true);
	lScrollArea->setFrameStyle(QFrame::NoFrame);

	auto lAddPlanButton = new QPushButton(QIcon::fromTheme(QStringLiteral("list-add")),
	                                              xi18nc("@action:button", "Add New Plan"));
	connect(lAddPlanButton, &QPushButton::clicked, this, [this]{
		mPlans.append(new BackupPlan(mPlans.count() + 1, mConfig, this));
		if(mBupVersion.isEmpty()) mPlans.last()->mBackupType = 1;
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
	connect(lFilediggerButton, &QPushButton::clicked, []{KProcess::startDetached(QStringLiteral("kup-filedigger"));});
	mVerticalLayout->addWidget(lFilediggerButton);
	mVerticalLayout->addStretch(1);
	lCentralWidget->setLayout(mVerticalLayout);
}

void KupKcm::createPlanWidgets(int pIndex) {
	auto lPlanWidget = new BackupPlanWidget(mPlans.at(pIndex), mBupVersion,
	                                         mRsyncVersion, mPar2Available);
	connect(lPlanWidget, SIGNAL(requestOverviewReturn()), this, SLOT(showFrontPage()));
	auto lConfigManager = new KConfigDialogManager(lPlanWidget, mPlans.at(pIndex));
	lConfigManager->setObjectName(objectName());
	connect(lConfigManager, SIGNAL(widgetModified()), this, SLOT(updateChangedStatus()));
	auto lStatusWidget = new PlanStatusWidget(mPlans.at(pIndex));
	connect(lStatusWidget, &PlanStatusWidget::removeMe, this, [this,pIndex]{
		if(pIndex < mSettings->mNumberOfPlans)
			partiallyRemovePlan(pIndex);
		else
			completelyRemovePlan(pIndex);
		updateChangedStatus();
	});
	connect(lStatusWidget, &PlanStatusWidget::configureMe, this, [this,pIndex]{
		mStackedLayout->setCurrentIndex(pIndex + 1);
	});
	connect(lStatusWidget, &PlanStatusWidget::duplicateMe, this, [this,pIndex]{
		auto lNewPlan = new BackupPlan(mPlans.count() + 1, mConfig, this);
		lNewPlan->copyFrom(*mPlans.at(pIndex));
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
	connect(mEnableCheckBox, &QCheckBox::toggled,
	        lStatusWidget, &PlanStatusWidget::setEnabled);
	connect(lPlanWidget->mDescriptionEdit, &KLineEdit::textChanged,
	        lStatusWidget->mDescriptionLabel, &QLabel::setText);

	mConfigManagers[pIndex] = lConfigManager;
	mPlanWidgets[pIndex] = lPlanWidget;
	mStackedLayout->insertWidget(pIndex + 1, lPlanWidget);
	mStatusWidgets[pIndex] = lStatusWidget;
	mVerticalLayout->insertWidget(pIndex, lStatusWidget);
}

void KupKcm::completelyRemovePlan(int pIndex) {
	mVerticalLayout->removeWidget(mStatusWidgets.at(pIndex));
	mStackedLayout->removeWidget(mPlanWidgets.at(pIndex));
	delete mConfigManagers.takeAt(pIndex);
	delete mStatusWidgets.takeAt(pIndex);
	delete mPlanWidgets.takeAt(pIndex);
	delete mPlans.takeAt(pIndex);
}

void KupKcm::partiallyRemovePlan(int pIndex) {
	mVerticalLayout->removeWidget(mStatusWidgets.at(pIndex));
	mStackedLayout->removeWidget(mPlanWidgets.at(pIndex));
	mConfigManagers.at(pIndex)->deleteLater();
	mConfigManagers[pIndex] = nullptr;
	mStatusWidgets.at(pIndex)->deleteLater();
	mStatusWidgets[pIndex] = nullptr;
	mPlanWidgets.at(pIndex)->deleteLater();
	mPlanWidgets[pIndex] = nullptr;
}

#include "kupkcm.moc"
