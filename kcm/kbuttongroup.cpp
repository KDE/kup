// SPDX-FileCopyrightText: 2006 Pino Toscano <toscano.pino@tiscali.it>
// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "kbuttongroup.h"

#include <QAbstractButton>
#include <QChildEvent>
#include <QHash>
#include <QSignalMapper>

class KButtonGroup::Private
{
public:
    explicit Private(KButtonGroup *q)
        : q(q)
        , currentId(-1)
        , nextId(0)
        , wantToBeId(-1)
    {
        connect(&clickedMapper, SIGNAL(mappedInt(int)), q, SLOT(slotClicked(int)));
        connect(&pressedMapper, SIGNAL(mappedInt(int)), q, SIGNAL(pressed(int)));
        connect(&releasedMapper, SIGNAL(mappedInt(int)), q, SIGNAL(released(int)));
    }

    void slotClicked(int id);

    KButtonGroup *q;
    QSignalMapper clickedMapper;
    QSignalMapper pressedMapper;
    QSignalMapper releasedMapper;

    QHash<QObject *, int> btnMap;
    int currentId;
    int nextId;
    int wantToBeId;
};

KButtonGroup::KButtonGroup(QWidget *parent)
    : QGroupBox(parent)
    , d(new Private(this))
{
}

KButtonGroup::~KButtonGroup()
{
    delete d;
}

void KButtonGroup::setSelected(int id)
{
    if (!testAttribute(Qt::WA_WState_Polished)) {
        d->wantToBeId = id;
        ensurePolished();
        return;
    }

    QHash<QObject *, int>::Iterator it = d->btnMap.begin();
    QHash<QObject *, int>::Iterator itEnd = d->btnMap.end();
    QAbstractButton *button = nullptr;

    for (; it != itEnd; ++it) {
        if ((it.value() == id) && (button = qobject_cast<QAbstractButton *>(it.key()))) {
            button->setChecked(true);
            d->currentId = id;
            emit changed(id);
            d->wantToBeId = -1;
            return;
        }
    }
    // button not found, it might still show up though, eg. because of premature polishing above
    d->wantToBeId = id;
}

int KButtonGroup::selected() const
{
    return d->currentId;
}

void KButtonGroup::childEvent(QChildEvent *event)
{
    if (event->polished()) {
        auto button = qobject_cast<QAbstractButton *>(event->child());
        if (!d->btnMap.contains(event->child()) && button) {
            connect(button, SIGNAL(clicked()), &d->clickedMapper, SLOT(map()));
            d->clickedMapper.setMapping(button, d->nextId);

            connect(button, SIGNAL(pressed()), &d->pressedMapper, SLOT(map()));
            d->pressedMapper.setMapping(button, d->nextId);

            connect(button, SIGNAL(released()), &d->releasedMapper, SLOT(map()));
            d->releasedMapper.setMapping(button, d->nextId);

            d->btnMap[button] = d->nextId;

            if (d->nextId == d->wantToBeId) {
                d->currentId = d->wantToBeId;
                d->wantToBeId = -1;
                button->setChecked(true);
                emit changed(d->currentId);
            }

            ++d->nextId;
        }
    } else if (event->removed()) {
        QObject *obj = event->child();
        QHash<QObject *, int>::ConstIterator it = d->btnMap.constFind(obj);

        if (it != d->btnMap.constEnd()) {
            d->clickedMapper.removeMappings(obj);
            d->pressedMapper.removeMappings(obj);
            d->releasedMapper.removeMappings(obj);

            if (it.value() == d->currentId) {
                d->currentId = -1;
            }

            d->btnMap.remove(obj);
        }
    }

    // be transparent
    QGroupBox::childEvent(event);
}

int KButtonGroup::id(QAbstractButton *button) const
{
    QHash<QObject *, int>::ConstIterator it = d->btnMap.constFind(button);

    if (it != d->btnMap.constEnd()) {
        return it.value();
    }

    return -1;
}

void KButtonGroup::Private::slotClicked(int id)
{
    currentId = id;
    emit q->clicked(id);
    emit q->changed(id);
}

#include "moc_kbuttongroup.cpp"
