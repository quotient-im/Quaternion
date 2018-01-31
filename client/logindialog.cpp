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

#include "lib/connection.h"

#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFormLayout>

#include "settings.h"

using QMatrixClient::Connection;

LoginDialog::LoginDialog(QWidget* parent)
    : Dialog(tr("Login"), parent, Dialog::StatusLine, tr("Login"),
             Dialog::NoExtraButtons)
    , serverEdit(new QLineEdit("https://matrix.org"))
    , userEdit(new QLineEdit(this))
    , passwordEdit(new QLineEdit(this))
    , initialDeviceName(new QLineEdit(this))
    , saveTokenCheck(new QCheckBox(tr("Stay logged in"), this))
    , m_connection(new Connection)
{

    passwordEdit->setEchoMode( QLineEdit::Password );

    auto* formLayout = addLayout<QFormLayout>();
    formLayout->addRow(tr("Matrix ID"), userEdit);
    formLayout->addRow(tr("Password"), passwordEdit);
    formLayout->addRow(tr("Device name"), initialDeviceName);
    formLayout->addRow(tr("Connect to server"), serverEdit);
    formLayout->addRow(saveTokenCheck);

    setPendingApplyMessage(tr("Connecting and logging in, please wait"));

    connect( userEdit, &QLineEdit::editingFinished, m_connection.data(),
             [=] {
                 auto userId = userEdit->text();
                 if (userId.startsWith('@') && userId.indexOf(':') != -1)
                     m_connection->resolveServer(userId);
             });
    connect( m_connection.data(), &Connection::homeserverChanged, serverEdit,
             [=] (QUrl newUrl)
             {
                 serverEdit->setText(newUrl.toString());
             });

    {
        // Fill defaults
        using namespace QMatrixClient;
        QStringList accounts { SettingsGroup("Accounts").childGroups() };
        if ( !accounts.empty() )
        {
            AccountSettings account { accounts.front() };
            userEdit->setText(account.userId());

            auto homeserver = account.homeserver();
            if (!homeserver.isEmpty())
            {
                m_connection->setHomeserver(homeserver);
                serverEdit->setText(homeserver.toString());
            }
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
    if (!serverEdit->text().isEmpty() && !serverEdit->text().startsWith("http"))
        url.setScheme("https"); // Qt defaults to http (or even ftp for some)
    m_connection->setHomeserver(url);
    connect( m_connection.data(), &Connection::connected,
             this, &Dialog::accept );
    connect( m_connection.data(), &Connection::loginError,
             this, &Dialog::applyFailed);
    m_connection->connectToServer(userEdit->text(), passwordEdit->text(),
                                  initialDeviceName->text());
}
