// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "versionlistdelegate.h"
#include "versionlistmodel.h"

#include <KFormat>
#include <KLocalizedString>

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QApplication>
#include <QGraphicsOpacityEffect>
#include <QMouseEvent>
#include <QPainter>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QtMath>
#include <utility>

static const int cMargin = 4;

Button::Button(QString pText, QWidget *pParent)
{
    // intentionally don't make this QObject owned by pParent
    mStyleOption.initFrom(pParent);
    mStyleOption.features = QStyleOptionButton::None;
    mStyleOption.state = QStyle::State_Enabled;
    mStyleOption.text = std::move(pText);

    const QSize lContentsSize = mStyleOption.fontMetrics.size(Qt::TextSingleLine, mStyleOption.text);
    mStyleOption.rect = QRect(QPoint(0, 0), QApplication::style()->sizeFromContents(QStyle::CT_PushButton, &mStyleOption, lContentsSize));
    mPushed = false;
    mParent = pParent;
}

void Button::setPosition(const QPoint &pTopRight)
{
    mStyleOption.rect.moveTopRight(pTopRight);
}

void Button::paint(QPainter *pPainter, float pOpacity)
{
    pPainter->setOpacity(static_cast<qreal>(pOpacity));
    QApplication::style()->drawControl(QStyle::CE_PushButton, &mStyleOption, pPainter);
}

bool Button::event(QEvent *pEvent)
{
    auto lMouseEvent = dynamic_cast<QMouseEvent *>(pEvent);
    bool lActivated = false;
    switch (pEvent->type()) {
    case QEvent::MouseMove:
        if (mStyleOption.rect.contains(lMouseEvent->pos())) {
            if (!(mStyleOption.state & QStyle::State_MouseOver)) {
                mStyleOption.state |= QStyle::State_MouseOver;
                if (mPushed) {
                    mStyleOption.state |= QStyle::State_Sunken;
                    mStyleOption.state &= ~QStyle::State_Raised;
                }
                mParent->update(mStyleOption.rect);
            }
        } else {
            if (mStyleOption.state & QStyle::State_MouseOver) {
                mStyleOption.state &= ~QStyle::State_MouseOver;
                if (mPushed) {
                    mStyleOption.state &= ~QStyle::State_Sunken;
                    mStyleOption.state |= QStyle::State_Raised;
                }
                mParent->update(mStyleOption.rect);
            }
        }
        break;
    case QEvent::MouseButtonPress:
        if (lMouseEvent->button() == Qt::LeftButton && !mPushed && (mStyleOption.state & QStyle::State_MouseOver)) {
            mPushed = true;
            mStyleOption.state |= QStyle::State_Sunken;
            mStyleOption.state &= ~QStyle::State_Raised;
            mParent->update(mStyleOption.rect);
        }
        break;
    case QEvent::MouseButtonRelease:
        if (lMouseEvent->button() == Qt::LeftButton) {
            if (mPushed && (mStyleOption.state & QStyle::State_MouseOver)) {
                lActivated = true;
            }
            mPushed = false;
            mStyleOption.state &= ~QStyle::State_Sunken;
            mStyleOption.state |= QStyle::State_Raised;
            mParent->update(mStyleOption.rect);
        }
        break;
    case QEvent::KeyPress: {
        auto lKeyEvent = dynamic_cast<QKeyEvent *>(pEvent);
        if ((mStyleOption.state & QStyle::State_HasFocus)) {
            if (lKeyEvent->key() == Qt::Key_Left || lKeyEvent->key() == Qt::Key_Right) {
                mStyleOption.state &= ~QStyle::State_HasFocus;
                emit focusChangeRequested(lKeyEvent->key() == Qt::Key_Right);
                mParent->update(mStyleOption.rect);
            } else if (lKeyEvent->key() == Qt::Key_Space || lKeyEvent->key() == Qt::Key_Return || lKeyEvent->key() == Qt::Key_Enter) {
                lActivated = true;
            }
        }
        break;
    }
    default:
        break;
    }
    return lActivated;
}

VersionItemAnimation::VersionItemAnimation(QWidget *pParent)
    : QParallelAnimationGroup(pParent)
{
    mParent = pParent;
    mExtraHeight = 0;
    mOpacity = 0;
    mOpenButton = new Button(xi18nc("@action:button", "Open"), pParent);
    connect(mOpenButton, SIGNAL(focusChangeRequested(bool)), SLOT(changeFocus(bool)), Qt::QueuedConnection);
    mRestoreButton = new Button(xi18nc("@action:button", "Restore"), pParent);
    connect(mRestoreButton, SIGNAL(focusChangeRequested(bool)), SLOT(changeFocus(bool)), Qt::QueuedConnection);
    auto lHeightAnimation = new QPropertyAnimation(this, "extraHeight", this);
    lHeightAnimation->setStartValue(0.0);
    lHeightAnimation->setEndValue(1.0);
    lHeightAnimation->setDuration(300);
    lHeightAnimation->setEasingCurve(QEasingCurve::InOutBack);
    addAnimation(lHeightAnimation);

    auto lWidgetOpacityAnimation = new QPropertyAnimation(this, "opacity", this);
    lWidgetOpacityAnimation->setStartValue(0.0);
    lWidgetOpacityAnimation->setEndValue(1.0);
    lWidgetOpacityAnimation->setDuration(300);
    addAnimation(lWidgetOpacityAnimation);
}

void VersionItemAnimation::setExtraHeight(qreal pExtraHeight)
{
    mExtraHeight = pExtraHeight;
    emit sizeChanged(mIndex);
}

void VersionItemAnimation::changeFocus(bool pForward)
{
    Q_UNUSED(pForward)
    if (sender() == mOpenButton) {
        mRestoreButton->mStyleOption.state |= QStyle::State_HasFocus;
        mParent->update(mRestoreButton->mStyleOption.rect);
    } else if (sender() == mRestoreButton) {
        mOpenButton->mStyleOption.state |= QStyle::State_HasFocus;
        mParent->update(mOpenButton->mStyleOption.rect);
    }
}

void VersionItemAnimation::setFocus(bool pFocused)
{
    if (!pFocused) {
        mOpenButton->mStyleOption.state &= ~QStyle::State_HasFocus;
        mRestoreButton->mStyleOption.state &= ~QStyle::State_HasFocus;
    } else {
        mOpenButton->mStyleOption.state |= QStyle::State_HasFocus;
        mRestoreButton->mStyleOption.state &= ~QStyle::State_HasFocus;
    }
    mParent->update(mOpenButton->mStyleOption.rect);
    mParent->update(mRestoreButton->mStyleOption.rect);
}

VersionListDelegate::VersionListDelegate(QAbstractItemView *pItemView, QObject *pParent)
    : QAbstractItemDelegate(pParent)
{
    mView = pItemView;
    mModel = pItemView->model();
    connect(pItemView->selectionModel(), &QItemSelectionModel::currentChanged, this, &VersionListDelegate::updateCurrent);
    connect(pItemView->model(), &QAbstractItemModel::modelReset, this, &VersionListDelegate::reset);
    pItemView->viewport()->installEventFilter(this); // mouse events
    pItemView->installEventFilter(this); // keyboard events
    pItemView->viewport()->setMouseTracking(true);
}

VersionListDelegate::~VersionListDelegate() = default;

void VersionListDelegate::paint(QPainter *pPainter, const QStyleOptionViewItem &pOption, const QModelIndex &pIndex) const
{
    QStyle *lStyle = QApplication::style();
    lStyle->drawPrimitive(QStyle::PE_PanelItemViewItem, &pOption, pPainter);
    pPainter->save();
    pPainter->setPen(pOption.palette.color((pOption.state & QStyle::State_HasFocus) ? QPalette::HighlightedText : QPalette::Text));
    QRect lMarginRect = pOption.rect.adjusted(cMargin, cMargin, -cMargin, -cMargin);

    QRect lSizeDisplayBounds;
    if (!pIndex.data(VersionIsDirectoryRole).toBool()) {
        QString lSizeText = KFormat().formatByteSize(static_cast<double>(pIndex.data(VersionSizeRole).toULongLong()));
        pPainter->drawText(lMarginRect, Qt::AlignRight | Qt::AlignTop, lSizeText, &lSizeDisplayBounds);
    }
    QString lDateText = pOption.fontMetrics.elidedText(pIndex.data().toString(), Qt::ElideRight, lMarginRect.width() - lSizeDisplayBounds.width());
    pPainter->drawText(lMarginRect, Qt::AlignLeft | Qt::AlignTop, lDateText);
    pPainter->restore();

    VersionItemAnimation *lAnimation = mActiveAnimations.value(pIndex);
    if (lAnimation != nullptr) {
        pPainter->save();
        pPainter->setClipRect(pOption.rect);

        lAnimation->mRestoreButton->setPosition(pOption.rect.topRight() + QPoint(-cMargin, pOption.fontMetrics.height() + 2 * cMargin));
        lAnimation->mRestoreButton->paint(pPainter, lAnimation->opacity());

        lAnimation->mOpenButton->setPosition(lAnimation->mRestoreButton->mStyleOption.rect.topLeft() + QPoint(-cMargin, 0));
        lAnimation->mOpenButton->paint(pPainter, lAnimation->opacity());
        pPainter->restore();
    }
}

QSize VersionListDelegate::sizeHint(const QStyleOptionViewItem &pOption, const QModelIndex &pIndex) const
{
    int lExtraHeight = 0;
    int lExtraWidth = 0;
    VersionItemAnimation *lAnimation = mActiveAnimations.value(pIndex);
    if (lAnimation != nullptr) {
        int lButtonHeight = lAnimation->mOpenButton->mStyleOption.rect.height();
        lExtraHeight = qCeil(lAnimation->extraHeight() * (lButtonHeight + cMargin));
        lExtraWidth = lAnimation->mOpenButton->mStyleOption.rect.width() + lAnimation->mRestoreButton->mStyleOption.rect.width();
    }
    return {lExtraWidth, cMargin * 2 + pOption.fontMetrics.height() + lExtraHeight};
}

bool VersionListDelegate::eventFilter(QObject *pObject, QEvent *pEvent)
{
    foreach (VersionItemAnimation *lAnimation, mActiveAnimations) {
        if (lAnimation->mOpenButton->event(pEvent)) {
            emit openRequested(lAnimation->mIndex);
        }
        if (lAnimation->mRestoreButton->event(pEvent)) {
            emit restoreRequested(lAnimation->mIndex);
        }
    }
    return QAbstractItemDelegate::eventFilter(pObject, pEvent);
}

void VersionListDelegate::updateCurrent(const QModelIndex &pCurrent, const QModelIndex &pPrevious)
{
    if (pPrevious.isValid()) {
        VersionItemAnimation *lPrevAnim = mActiveAnimations.value(pPrevious);
        if (lPrevAnim != nullptr) {
            lPrevAnim->setDirection(QAbstractAnimation::Backward);
            lPrevAnim->start();
            lPrevAnim->setFocus(false);
        }
    }
    if (pCurrent.isValid()) {
        VersionItemAnimation *lCurAnim = mActiveAnimations.value(pCurrent);
        if (lCurAnim == nullptr) {
            if (!mInactiveAnimations.isEmpty()) {
                lCurAnim = mInactiveAnimations.takeFirst();
            } else {
                lCurAnim = new VersionItemAnimation(mView->viewport());
                connect(lCurAnim, SIGNAL(sizeChanged(QModelIndex)), SIGNAL(sizeHintChanged(QModelIndex)));
                connect(lCurAnim, SIGNAL(finished()), SLOT(reclaimAnimation()));
            }
            lCurAnim->mIndex = pCurrent;
            mActiveAnimations.insert(pCurrent, lCurAnim);
        }
        lCurAnim->setDirection(QAbstractAnimation::Forward);
        lCurAnim->start();
        lCurAnim->setFocus(true);
    }
}

void VersionListDelegate::reset()
{
    mInactiveAnimations.append(mActiveAnimations.values());
    mActiveAnimations.clear();
}

void VersionListDelegate::reclaimAnimation()
{
    auto lAnimation = qobject_cast<VersionItemAnimation *>(sender());
    if (lAnimation->direction() == QAbstractAnimation::Backward) {
        mInactiveAnimations.append(lAnimation);
        foreach (const VersionItemAnimation *lActiveAnimation, mActiveAnimations) {
            if (lActiveAnimation == lAnimation) {
                mActiveAnimations.remove(lAnimation->mIndex);
                break;
            }
        }
    }
}
