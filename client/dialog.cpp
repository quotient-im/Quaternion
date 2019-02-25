/******************************************************************************
 * Copyright (C) 2017 Kitsune Ral <kitsune-ral@users.sf.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "dialog.h"

#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>

Dialog::Dialog(const QString& title, QWidget *parent,
               UseStatusLine useStatusLine, const QString& applyTitle,
               QDialogButtonBox::StandardButtons addButtons)
    : Dialog(title
        , QDialogButtonBox::Ok | QDialogButtonBox::Cancel | addButtons
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

#if (QT_VERSION < QT_VERSION_CHECK(5, 10, 0))
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
#endif

    connect(buttons, &QDialogButtonBox::clicked, this, &Dialog::buttonClicked);

    outerLayout.addWidget(buttons);
    if (statusLabel)
        outerLayout.addWidget(statusLabel);
}

void Dialog::addLayout(QLayout* l)
{
    int offset = 1 + (statusLabel != nullptr);
    outerLayout.insertLayout(outerLayout.count() - offset, l);
}

void Dialog::addWidget(QWidget* w)
{
    int offset = 1 + (statusLabel != nullptr);
    outerLayout.insertWidget(outerLayout.count() - offset, w);
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
