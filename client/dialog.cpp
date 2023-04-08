/*
 * SPDX-FileCopyrightText: 2017 Kitsune Ral <kitsune-ral@users.sf.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "dialog.h"

#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>

Dialog::Dialog(const QString& title, QWidget *parent,
               UseStatusLine useStatusLine, const QString& applyTitle,
               QDialogButtonBox::StandardButtons addButtons)
    : Dialog(title
        , QDialogButtonBox::Ok | /*QDialogButtonBox::Cancel |*/ addButtons
        , parent, useStatusLine)
{
    if (!applyTitle.isEmpty())
        buttons->button(QDialogButtonBox::Ok)->setText(applyTitle);
}


Dialog::Dialog(const QString& title, QDialogButtonBox::StandardButtons setButtons,
    QWidget *parent, UseStatusLine useStatusLine)
    : QDialog(parent)
    , applyLatency(useStatusLine)
    , pendingApplyMessage(tr("Applying changes, please wait"))
    , statusLabel(useStatusLine == NoStatusLine ? nullptr : new QLabel)
    , buttons(new QDialogButtonBox(setButtons))
    , outerLayout(this)
{
    setWindowTitle(title);

    connect(buttons, &QDialogButtonBox::clicked, this, &Dialog::buttonClicked);

    outerLayout.addWidget(buttons);
    if (statusLabel)
        outerLayout.addWidget(statusLabel);
}

void Dialog::addLayout(QLayout* l, int stretch)
{
    int offset = 1 + (statusLabel != nullptr);
    outerLayout.insertLayout(outerLayout.count() - offset, l, stretch);
}

void Dialog::addWidget(QWidget* w, int stretch, Qt::Alignment alignment)
{
    int offset = 1 + (statusLabel != nullptr);
    outerLayout.insertWidget(outerLayout.count() - offset, w, stretch, alignment);
}

QPushButton*Dialog::button(QDialogButtonBox::StandardButton which)
{
    return buttonBox()->button(which);
}

void Dialog::reactivate()
{
    if (!isVisible())
    {
        load();
        show();
    }
    raise();
    activateWindow();
}

void Dialog::setStatusMessage(const QString& msg)
{
    Q_ASSERT(statusLabel);
    statusLabel->setText(msg);
}

void Dialog::applyFailed(const QString& errorMessage)
{
    setStatusMessage(errorMessage);
    setDisabled(false);
}

void Dialog::buttonClicked(QAbstractButton* button)
{
    switch (buttons->buttonRole(button))
    {
        case QDialogButtonBox::AcceptRole:
        case QDialogButtonBox::YesRole:
            if (validate())
            {
                if (statusLabel)
                    statusLabel->setText(pendingApplyMessage);
                setDisabled(true);
                apply();
            }
            break;
        case QDialogButtonBox::ResetRole:
            load();
            break;
        case QDialogButtonBox::RejectRole:
        case QDialogButtonBox::NoRole:
            reject();
            break;
        default:
            ; // Derived classes may completely replace or reuse this method
    }
}
