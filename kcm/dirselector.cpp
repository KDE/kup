// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "dirselector.h"

#include <KDirLister>
#include <KDirModel>
#include <KFileUtils>
#include <KLocalizedString>
#include <KMessageBox>

#include <QDir>
#include <QInputDialog>

DirSelector::DirSelector(QWidget *pParent)
    : QTreeView(pParent)
{
    mDirModel = new KDirModel(this);
    mDirModel->dirLister()->setDirOnlyMode(true);
    setModel(mDirModel);
    for (int i = 1; i < mDirModel->columnCount(); ++i) {
        hideColumn(i);
    }
    setHeaderHidden(true);
    connect(mDirModel, SIGNAL(expand(QModelIndex)), SLOT(expand(QModelIndex)));
    connect(mDirModel, SIGNAL(expand(QModelIndex)), SLOT(selectEntry(QModelIndex)));
}

QUrl DirSelector::url() const
{
    const KFileItem lFileItem = mDirModel->itemForIndex(currentIndex());
    return !lFileItem.isNull() ? lFileItem.url() : QUrl();
}

void DirSelector::createNewFolder()
{
    bool lUserAccepted;
    QString lNameSuggestion = xi18nc("default folder name when creating a new folder", "New Folder");
    if (QFileInfo::exists(url().adjusted(QUrl::StripTrailingSlash).path() + '/' + lNameSuggestion)) {
        lNameSuggestion = KFileUtils::suggestName(url(), lNameSuggestion);
    }

    QString lSelectedName = QInputDialog::getText(this,
                                                  xi18nc("@title:window", "New Folder"),
                                                  xi18nc("@label:textbox", "Create new folder in:\n%1", url().path()),
                                                  QLineEdit::Normal,
                                                  lNameSuggestion,
                                                  &lUserAccepted);
    if (!lUserAccepted)
        return;

    QUrl lPartialUrl(url());
    const QStringList lDirectories = lSelectedName.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    foreach (QString lSubDirectory, lDirectories) {
        QDir lDir(lPartialUrl.path());
        if (lDir.exists(lSubDirectory)) {
            lPartialUrl = lPartialUrl.adjusted(QUrl::StripTrailingSlash);
            lPartialUrl.setPath(lPartialUrl.path() + '/' + (lSubDirectory));
            KMessageBox::error(this, i18n("A folder named %1 already exists.", lPartialUrl.path()));
            return;
        }
        if (!lDir.mkdir(lSubDirectory)) {
            lPartialUrl = lPartialUrl.adjusted(QUrl::StripTrailingSlash);
            lPartialUrl.setPath(lPartialUrl.path() + '/' + (lSubDirectory));
            KMessageBox::error(this, i18n("You do not have permission to create %1.", lPartialUrl.path()));
            return;
        }
        lPartialUrl = lPartialUrl.adjusted(QUrl::StripTrailingSlash);
        lPartialUrl.setPath(lPartialUrl.path() + '/' + (lSubDirectory));
    }
    mDirModel->expandToUrl(lPartialUrl);
}

void DirSelector::selectEntry(QModelIndex pIndex)
{
    selectionModel()->clearSelection();
    selectionModel()->setCurrentIndex(pIndex, QItemSelectionModel::SelectCurrent);
    scrollTo(pIndex);
}

void DirSelector::expandToUrl(const QUrl &pUrl)
{
    mDirModel->expandToUrl(pUrl);
}

void DirSelector::setRootUrl(const QUrl &pUrl)
{
    mDirModel->dirLister()->openUrl(pUrl);
}
