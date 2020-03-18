/**************************************************************************
 *                                                                        *
 * Copyright (C) 2015 Felix Rohrbach <kde@fxrh.de>                        *
 *                                                                        *
 * This program is free software; you can redistribute it and/or          *
 * modify it under the terms of the GNU General Public License            *
 * as published by the Free Software Foundation; either version 3         *
 * of the License, or (at your option) any later version.                 *
 *                                                                        *
 * This program is distributed in the hope that it will be useful,        *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 * GNU General Public License for more details.                           *
 *                                                                        *
 * You should have received a copy of the GNU General Public License      *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 *                                                                        *
 **************************************************************************/

#include "logindialog.h"

#include <connection.h>
#include <settings.h>

#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFormLayout>

using Quotient::Connection;

LoginDialog::LoginDialog(QWidget* parent, const QStringList& knownAccounts)
    : Dialog(tr("Login"), parent, Dialog::StatusLine, tr("Login"),
             Dialog::NoExtraButtons)
    , userEdit(new QLineEdit(this))
    , passwordEdit(new QLineEdit(this))
    , initialDeviceName(new QLineEdit(this))
    , serverEdit(new QLineEdit(QStringLiteral("https://matrix.org"), this))
    , saveTokenCheck(new QCheckBox(tr("Stay logged in"), this))
    , m_connection(new Connection)
{
    setup();
    setPendingApplyMessage(tr("Connecting and logging in, please wait"));

    connect(userEdit, &QLineEdit::editingFinished, m_connection.data(),
            [=] {
                auto userId = userEdit->text();
                if (userId.startsWith('@') && userId.indexOf(':') != -1)
                    m_connection->resolveServer(userId);
            });

    {
        // Fill defaults
        using namespace Quotient;
        if ( !knownAccounts.empty() )
        {
            AccountSettings account { knownAccounts.front() };
            userEdit->setText(account.userId());

            auto homeserver = account.homeserver();
            if (!homeserver.isEmpty())
                m_connection->setHomeserver(homeserver);
//                serverEdit->setText(homeserver.toString());

            initialDeviceName->setText(account.deviceName());
            saveTokenCheck->setChecked(account.keepLoggedIn());
            passwordEdit->setFocus();
        }
        else
        {
            saveTokenCheck->setChecked(false);
            userEdit->setFocus();
        }
    }
}

LoginDialog::LoginDialog(QWidget* parent,
                         const Quotient::AccountSettings& reloginData)
    : Dialog(tr("Re-login"), parent, Dialog::StatusLine, tr("Re-login"),
             Dialog::NoExtraButtons)
    , userEdit(new QLineEdit(reloginData.userId(), this))
    , passwordEdit(new QLineEdit(this))
    , initialDeviceName(new QLineEdit(reloginData.deviceName(), this))
    , serverEdit(new QLineEdit(reloginData.homeserver().toString(), this))
    , saveTokenCheck(new QCheckBox(tr("Stay logged in"), this))
    , m_connection(new Connection)
{
    setup();
    userEdit->setReadOnly(true);
    userEdit->setFrame(false);
    setPendingApplyMessage(tr("Restoring access, please wait"));
}

void LoginDialog::setup()
{
    passwordEdit->setEchoMode( QLineEdit::Password );

    connect(m_connection.data(), &Connection::homeserverChanged, serverEdit,
            [=] (const QUrl& newUrl) { serverEdit->setText(newUrl.toString()); });

    auto* formLayout = addLayout<QFormLayout>();
    formLayout->addRow(tr("Matrix ID"), userEdit);
    formLayout->addRow(tr("Password"), passwordEdit);
    formLayout->addRow(tr("Device name"), initialDeviceName);
    formLayout->addRow(tr("Connect to server"), serverEdit);
    formLayout->addRow(saveTokenCheck);
}

LoginDialog::~LoginDialog() = default;

Connection* LoginDialog::releaseConnection()
{
    return m_connection.take();
}

QString LoginDialog::deviceName() const
{
    return initialDeviceName->text();
}

bool LoginDialog::keepLoggedIn() const
{
    return saveTokenCheck->isChecked();
}

void LoginDialog::apply()
{
    auto url = QUrl::fromUserInput(serverEdit->text());
    if (!serverEdit->text().isEmpty() && !serverEdit->text().startsWith("http:"))
        url.setScheme("https"); // Qt defaults to http (or even ftp for some)
    m_connection->setHomeserver(url);
    connect( m_connection.data(), &Connection::connected,
             this, &Dialog::accept );
    connect( m_connection.data(), &Connection::loginError,
             this, &Dialog::applyFailed);
    m_connection->connectToServer(userEdit->text(), passwordEdit->text(),
                                  initialDeviceName->text());
}
