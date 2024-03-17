// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef DRIVESELECTIONDELEGATE_H
#define DRIVESELECTIONDELEGATE_H

#include <QStyledItemDelegate>

class QListView;
class KCapacityBar;

class DriveSelectionDelegate : public QStyledItemDelegate
{
public:
    explicit DriveSelectionDelegate(QListView *pParent);
    ~DriveSelectionDelegate() override;
    void paint(QPainter *pPainter, const QStyleOptionViewItem &pOption, const QModelIndex &pIndex) const override;
    QSize sizeHint(const QStyleOptionViewItem &pOption, const QModelIndex &pIndex) const override;

private:
    QRect warningRect(const QRect &pRect, const QModelIndex &pIndex) const;
    static QString warningText(const QModelIndex &pIndex);
    KCapacityBar *mCapacityBar;
    QListView *mListView;
};

#endif // DRIVESELECTIONDELEGATE_H
