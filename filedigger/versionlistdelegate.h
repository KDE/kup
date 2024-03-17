// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef VERSIONLISTDELEGATE_H
#define VERSIONLISTDELEGATE_H

#include <QAbstractItemDelegate>
#include <QParallelAnimationGroup>

class Button : public QObject
{
    Q_OBJECT

public:
    Button(QString pText, QWidget *pParent);
    bool mPushed;
    QStyleOptionButton mStyleOption;
    QWidget *mParent;

    void setPosition(const QPoint &pTopRight);
    void paint(QPainter *pPainter, float pOpacity);
    bool event(QEvent *pEvent) override;

signals:
    void focusChangeRequested(bool pForward);
};

class VersionItemAnimation : public QParallelAnimationGroup
{
    Q_OBJECT
    Q_PROPERTY(float extraHeight READ extraHeight WRITE setExtraHeight)
    Q_PROPERTY(float opacity READ opacity WRITE setOpacity)

public:
    explicit VersionItemAnimation(QWidget *pParent);
    ~VersionItemAnimation()
    {
        delete mOpenButton;
        delete mRestoreButton;
    }
    qreal extraHeight()
    {
        return mExtraHeight;
    }
    float opacity()
    {
        return mOpacity;
    }

signals:
    void sizeChanged(const QModelIndex &pIndex);

public slots:
    void setExtraHeight(qreal pExtraHeight);
    void setOpacity(float pOpacity)
    {
        mOpacity = pOpacity;
    }
    void changeFocus(bool pForward);
    void setFocus(bool pFocused);

public:
    QPersistentModelIndex mIndex;
    qreal mExtraHeight;
    float mOpacity;
    Button *mOpenButton;
    Button *mRestoreButton;
    QWidget *mParent;
};

class VersionListDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    explicit VersionListDelegate(QAbstractItemView *pItemView, QObject *pParent = nullptr);
    ~VersionListDelegate() override;
    void paint(QPainter *pPainter, const QStyleOptionViewItem &pOption, const QModelIndex &pIndex) const override;
    QSize sizeHint(const QStyleOptionViewItem &pOption, const QModelIndex &pIndex) const override;
    bool eventFilter(QObject *pObject, QEvent *pEvent) override;

signals:
    void openRequested(const QModelIndex &pIndex);
    void restoreRequested(const QModelIndex &pIndex);

public slots:
    void updateCurrent(const QModelIndex &pCurrent, const QModelIndex &pPrevious);
    void reset();
    void reclaimAnimation();

protected:
    void initialize();
    QAbstractItemView *mView;
    QAbstractItemModel *mModel;
    QHash<QPersistentModelIndex, VersionItemAnimation *> mActiveAnimations;
    QList<VersionItemAnimation *> mInactiveAnimations;
};

#endif // VERSIONLISTDELEGATE_H
