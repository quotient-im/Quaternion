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

#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFormLayout>

#include <QtCore/QUrl>

#include "quaternionconnection.h"
#include "settings.h"

class LoginDialog::AccountData
{
    public:
        QString name;
        QUrl homeserver;
        QString username;
        bool hasSavedToken;
};

Q_DECLARE_METATYPE(LoginDialog::AccountData);


LoginDialog::LoginDialog(QWidget* parent)
    : QDialog(parent)
    , m_connection(nullptr)
{
    accountBox = new QComboBox();
    connect(accountBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &LoginDialog::accountBoxChanged);
    serverEdit = new QLineEdit("https://matrix.org");
    userEdit = new QLineEdit();
    passwordEdit = new QLineEdit();
    passwordEdit->setEchoMode( QLineEdit::Password );
    saveTokenCheck = new QCheckBox("Stay logged in");
    defaultConnectionCheck = new QCheckBox(tr("Use this connection as default"));
    autoConnectCheck = new QCheckBox(tr("Autoconnect on startup"));
    statusLabel = new QLabel("Welcome to Quaternion");
    loginButton = new QPushButton("Login");

    QFormLayout* formLayout = new QFormLayout();
    formLayout->addRow(accountBox);
    formLayout->addRow("Server", serverEdit);
    formLayout->addRow("User", userEdit);
    formLayout->addRow("Password", passwordEdit);
    formLayout->addRow(saveTokenCheck);
    formLayout->addRow(defaultConnectionCheck);
    formLayout->addRow(autoConnectCheck);
    
    QVBoxLayout* mainLayout = new QVBoxLayout();
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(loginButton);
    mainLayout->addWidget(statusLabel);
    
    setLayout(mainLayout);
    
    // The checkboxes are ordered in such a way that the next checkbox should only be enabled
    // if the current one is checked. These two connections ensure that.
    connect(saveTokenCheck, &QCheckBox::stateChanged, [this](bool state){
        if( state )
        {
            defaultConnectionCheck->setEnabled(true);
        }
        else
        {
            defaultConnectionCheck->setEnabled(false);
            defaultConnectionCheck->setChecked(false);
            autoConnectCheck->setEnabled(false);
            autoConnectCheck->setChecked(false);
        }
    });
    connect(defaultConnectionCheck, &QCheckBox::stateChanged, [this](bool state){
        if( state )
        {
            autoConnectCheck->setEnabled(true);
        }
        else
        {
            autoConnectCheck->setEnabled(false);
            autoConnectCheck->setChecked(false);
        }
    });

    {
        // Fill defaults
        using namespace QMatrixClient;
        QStringList accounts { SettingsGroup("Accounts").childGroups() };
        for( QString accountName: accounts )
        {
            AccountSettings account { accountName };
            AccountData data;
            data.name = accountName;
            data.homeserver = account.homeserver();
            QString username = account.userId();
            if (username.startsWith('@'))
            {
                QString serverpart = ":" + data.homeserver.host();
                if (data.homeserver.port() != -1)
                    serverpart += ":" + QString::number(data.homeserver.port());
                if (username.endsWith(serverpart))
                {
                    // Keep only the local part of the user id
                    username.remove(0, 1).chop(serverpart.size());
                }
            }
            data.username = username;
            data.hasSavedToken = account.keepLoggedIn();
            accountBox->addItem(accountName, QVariant::fromValue<AccountData>(data));
        }
        accountBox->addItem(tr("Add new account..."));
        accountBox->setCurrentIndex(0);
    }
    if (userEdit->text().isEmpty())
        userEdit->setFocus();
    else
        passwordEdit->setFocus();

    connect( loginButton, &QPushButton::clicked, this, &LoginDialog::login );
}

void LoginDialog::setStatusMessage(const QString& msg)
{
    statusLabel->setText(msg);
}

QuaternionConnection* LoginDialog::connection() const
{
    return m_connection;
}

bool LoginDialog::keepLoggedIn() const
{
    return saveTokenCheck->isChecked();
}

bool LoginDialog::isDefaultConnection() const
{
    return defaultConnectionCheck->isChecked();
}

bool LoginDialog::connectOnStartup() const
{
    return autoConnectCheck->isChecked();
}

void LoginDialog::accountBoxChanged(int index)
{
    QVariant data = accountBox->itemData(index);
    if( data.isValid() )
    {
        AccountData accountData = qvariant_cast<AccountData>(data);
        serverEdit->setText(accountData.homeserver.toString());
        serverEdit->setEnabled(false);
        userEdit->setText(accountData.username);
        userEdit->setEnabled(false);
        if( accountData.hasSavedToken )
        {
            passwordEdit->setText(tr("<i>token saved</i>"));
            passwordEdit->setEnabled(false);
            saveTokenCheck->setChecked(true);
        }
        else
        {
            passwordEdit->clear();
            passwordEdit->setEnabled(true);
            saveTokenCheck->setChecked(false);
        }
    }
    else
    {
        serverEdit->setText("https://matrix.org");
        serverEdit->setEnabled(true);
        userEdit->clear();
        userEdit->setEnabled(true);
        passwordEdit->clear();
        passwordEdit->setEnabled(true);
        saveTokenCheck->setChecked(false);
    }
    
}

void LoginDialog::login()
{
    setStatusMessage("Connecting and logging in, please wait");
    setDisabled(true);
    QUrl url = QUrl::fromUserInput(serverEdit->text());
    QString user = userEdit->text();
    QString password = passwordEdit->text();

    setConnection(new QuaternionConnection(url));

    connect( m_connection, &QMatrixClient::Connection::connected, this, &QDialog::accept );
    connect( m_connection, &QMatrixClient::Connection::loginError, this, &LoginDialog::error );
    m_connection->connectToServer(user, password);
}

void LoginDialog::error(QString error)
{
    setStatusMessage( error );
    setConnection(nullptr);
    setDisabled(false);
}

void LoginDialog::setConnection(QuaternionConnection* connection)
{
    if (m_connection != nullptr) {
        m_connection->deleteLater();
    }

    m_connection = connection;
}
