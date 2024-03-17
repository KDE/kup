// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef DIRSELECTOR_H
#define DIRSELECTOR_H

#include <QTreeView>

class KDirModel;

class DirSelector : public QTreeView
{
    Q_OBJECT
public:
    explicit DirSelector(QWidget *pParent = nullptr);
    QUrl url() const;

signals:

public slots:
    void createNewFolder();
    void selectEntry(QModelIndex pIndex);
    void expandToUrl(const QUrl &pUrl);
    void setRootUrl(const QUrl &pUrl);

private:
    KDirModel *mDirModel;
};

#endif // DIRSELECTOR_H
