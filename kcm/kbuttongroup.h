// SPDX-FileCopyrightText: 2006 Pino Toscano <toscano.pino@tiscali.it>
// SPDX-FileCopyrightText: 2020 Simon Persson <simon.persson@mykolab.com>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef KBUTTONGROUP_H
#define KBUTTONGROUP_H

#include <QGroupBox>

class QAbstractButton;

/**
 * @deprecated since 5.0, use QGroupBox and QButtonGroup instead
 *
 * @short Group box with index of the selected button
 * KButtonGroup is a simple group box that can keep track of the current selected
 * button of the ones added to it.
 *
 * Use normally as you would with a QGroupBox.
 *
 * \image html kbuttongroup.png "KDE Button Group containing 3 KPushButtons"
 *
 * @author Pino Toscano <toscano.pino@tiscali.it>
 */
class KButtonGroup : public QGroupBox
{
    Q_OBJECT

    Q_PROPERTY(int current READ selected WRITE setSelected NOTIFY changed USER true)

public:
    /**
     * Construct a new empty KGroupBox.
     */
    explicit KButtonGroup(QWidget *parent = nullptr);

    /**
     * Destroys the widget.
     */
    ~KButtonGroup() override;

    /**
     * Return the index of the selected QAbstractButton, among the QAbstractButton's
     * added to the widget.
     * @return the index of the selected button
     */
    int selected() const;

    /**
     * @return the index of @p button.
     * @since 4.3
     */
    int id(QAbstractButton *button) const;

public Q_SLOTS:
    /**
     * Select the \p id -th button
     */
    void setSelected(int id);

Q_SIGNALS:
    /**
     * The button with index \p id was clicked
     */
    void clicked(int id);
    /**
     * The button with index \p id was pressed
     */
    void pressed(int id);
    /**
     * The button with index \p id was released
     */
    void released(int id);
    /**
     * Emitted when anything (a click on a button, or calling setSelected())
     * change the id of the current selected. \p id is the index of the new
     * selected button.
     */
    void changed(int id);

protected:
    /**
     * Reimplemented from QGroupBox.
     */
    void childEvent(QChildEvent *event) override;

private:
    Q_PRIVATE_SLOT(d, void slotClicked(int id))

    class Private;
    friend class Private;
    Private *const d;
};

#endif
