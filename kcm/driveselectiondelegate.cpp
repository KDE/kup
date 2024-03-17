// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "driveselectiondelegate.h"
#include "driveselection.h"

#include <QApplication>
#include <QIcon>
#include <QPainter>
#include <QStyle>

#include <KCapacityBar>
#include <KFormat>
#include <KIO/Global>
#include <KLocalizedString>

static const int cMargin = 6;

DriveSelectionDelegate::DriveSelectionDelegate(QListView *pParent)
    : QStyledItemDelegate(pParent)
    , mCapacityBar(new KCapacityBar(KCapacityBar::DrawTextInline))
    , mListView(pParent)
{
}

DriveSelectionDelegate::~DriveSelectionDelegate()
{
    delete mCapacityBar;
}

void DriveSelectionDelegate::paint(QPainter *pPainter, const QStyleOptionViewItem &pOption, const QModelIndex &pIndex) const
{
    pPainter->save();
    pPainter->setRenderHint(QPainter::Antialiasing);
    QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &pOption, pPainter);

    auto lTotalSize = pIndex.data(DriveSelection::TotalSpace).toULongLong();
    auto lUsedSize = pIndex.data(DriveSelection::UsedSpace).toULongLong();
    bool lIsDisconnected = pIndex.data(DriveSelection::UDI).toString().isEmpty();

    auto lFontMetrics = mListView->fontMetrics();

    if (lTotalSize == 0 || lIsDisconnected) {
        mCapacityBar->setValue(0);
    } else {
        mCapacityBar->setValue(static_cast<int>((lUsedSize * 100) / lTotalSize));
    }
    mCapacityBar->drawCapacityBar(
        pPainter,
        pOption.rect.adjusted(cMargin, cMargin + lFontMetrics.height() + cMargin, -cMargin, 4 * cMargin + lFontMetrics.height() - pOption.rect.height()));

    if (pOption.state & QStyle::State_HasFocus)
        pPainter->setPen(pOption.palette.color(QPalette::HighlightedText));
    else
        pPainter->setPen(pOption.palette.color(QPalette::Text));

    KFormat lFormat;
    QString lDisplayLabel, lPartitionLabel, lDisconnectedLabel;
    int lTextEnd = pOption.rect.right() - cMargin;
    if (lIsDisconnected) {
        lDisconnectedLabel = xi18nc("@item:inlistbox this text is added if selected drive is disconnected", " (disconnected)");
    } else {
        lDisconnectedLabel = QString();
        if (lTotalSize > 0) {
            QString lFreeSpace = xi18nc("@label %1 is amount of free storage space of hard drive", "%1 free", lFormat.formatByteSize(lTotalSize - lUsedSize));
            int lTextWidth = lFontMetrics.horizontalAdvance(lFreeSpace);
            lTextEnd -= lTextWidth + cMargin;
            QPoint lOffset = QPoint(-cMargin - lTextWidth, cMargin + lFontMetrics.height());
            pPainter->drawText(pOption.rect.topRight() + lOffset, lFreeSpace);
        }
    }

    QString lDeviceDescription = pIndex.data(DriveSelection::DeviceDescription).toString();
    QString lLabel = pIndex.data(DriveSelection::Label).toString();
    int lPartitionNumber = pIndex.data(DriveSelection::PartitionNumber).toInt();
    if (lLabel.isEmpty() || lLabel == lDeviceDescription) {
        if (pIndex.data(DriveSelection::PartitionsOnDrive).toInt() > 1) {
            lPartitionLabel = xi18nc(
                "@item:inlistbox used for unnamed filesystems, more than one filesystem on device. %1 is partition number, %2 is device description, %3 is "
                "either empty or the \" (disconnected)\" text",
                "Partition %1 on %2%3",
                lPartitionNumber,
                lDeviceDescription,
                lDisconnectedLabel);
        } else {
            lPartitionLabel = xi18nc(
                "@item:inlistbox used when there is only one unnamed filesystem on device. %1 is device description, %2 is either empty or the \" "
                "(disconnected)\" text",
                "%1%2",
                lDeviceDescription,
                lDisconnectedLabel);
        }
    } else {
        lPartitionLabel = xi18nc("@item:inlistbox %1 is filesystem label, %2 is the device description, %3 is either empty or the \" (disconnected)\" text",
                                 "%1 on %2%3",
                                 lLabel,
                                 lDeviceDescription,
                                 lDisconnectedLabel);
    }

    if (lTotalSize == 0) {
        lDisplayLabel = lPartitionLabel;
    } else {
        lDisplayLabel = xi18nc("@item:inlistbox %1 is drive(partition) label, %2 is storage capacity",
                               "%1: %2 total capacity",
                               lPartitionLabel,
                               lFormat.formatByteSize(lTotalSize));
    }
    lDisplayLabel = lFontMetrics.elidedText(lDisplayLabel, Qt::ElideMiddle, lTextEnd - pOption.rect.left() - cMargin);
    pPainter->drawText(pOption.rect.topLeft() + QPoint(cMargin, cMargin + lFontMetrics.height()), lDisplayLabel);

    int lIconSize = 48;
    QRect lWarningRect = warningRect(pOption.rect.adjusted(lIconSize + cMargin, 0, 0, 0), pIndex);
    if (!lWarningRect.isEmpty()) {
        QIcon lIcon = QIcon::fromTheme(QStringLiteral("dialog-warning"));
        lIcon.paint(pPainter, lWarningRect.left() - cMargin - lIconSize, lWarningRect.top(), lIconSize, lIconSize);
        pPainter->drawText(lWarningRect, Qt::AlignVCenter | Qt::TextWordWrap, warningText(pIndex));
    }

    pPainter->restore();
}

QSize DriveSelectionDelegate::sizeHint(const QStyleOptionViewItem &pOption, const QModelIndex &pIndex) const
{
    Q_UNUSED(pOption)
    auto lFontMetrics = mListView->fontMetrics();
    QSize lSize;
    lSize.setWidth(cMargin * 2 + lFontMetrics.horizontalAdvance(pIndex.data().toString()));
    lSize.setHeight(cMargin * 5 + lFontMetrics.height());
    int lIconSize = 48;
    QRect lWarningRect = warningRect(mListView->rect().adjusted(lIconSize + cMargin, 0, 0, 0), pIndex);
    if (!lWarningRect.isEmpty()) {
        lSize.setHeight(lSize.height() + 2 * cMargin + lWarningRect.height());
    }
    return lSize;
}

QRect DriveSelectionDelegate::warningRect(const QRect &pRect, const QModelIndex &pIndex) const
{
    auto lFontMetrics = mListView->fontMetrics();
    QRect lTextLocation = pRect.adjusted(cMargin, 5 * cMargin + lFontMetrics.height(), -cMargin, -cMargin);
    QString lWarningText = warningText(pIndex);
    if (lWarningText.isEmpty()) {
        return {};
    }
    QRect lTextBoundary = lFontMetrics.boundingRect(lTextLocation, Qt::TextWordWrap, lWarningText);
    int lIconSize = 48;
    if (lTextBoundary.height() < lIconSize) {
        lTextBoundary.setHeight(lIconSize);
    }
    return lTextBoundary;
}

QString DriveSelectionDelegate::warningText(const QModelIndex &pIndex)
{
    bool lPermissionWarning = pIndex.data(DriveSelection::PermissionLossWarning).toBool();
    bool lSymlinkWarning = pIndex.data(DriveSelection::SymlinkLossWarning).toBool();
    if (lPermissionWarning && lSymlinkWarning) {
        return xi18nc("@item:inlistbox",
                      "Warning: Symbolic links and file permissions can not be saved "
                      "to this file system. File permissions only matters if there is more than one "
                      "user of this computer or if you are backing up executable program files.");
    }
    if (lPermissionWarning) {
        return xi18nc("@item:inlistbox",
                      "Warning: File permissions can not be saved to this file "
                      "system. File permissions only matters if there is more than one "
                      "user of this computer or if you are backing up executable program files.");
    }
    return {};
}
