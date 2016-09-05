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

#include <QtCore/QDebug>
#include <QtCore/QUrl>
#include <QtWidgets/QFormLayout>

#include "quaternionconnection.h"

LoginDialog::LoginDialog(QWidget* parent)
    : QDialog(parent)
{
    m_connection = nullptr;
    
    serverEdit = new QLineEdit("https://matrix.org");
    userEdit = new QLineEdit();
    passwordEdit = new QLineEdit();
    passwordEdit->setEchoMode( QLineEdit::Password );
    sessionLabel = new QLabel("Session:");
    loginButton = new QPushButton("Login");
    saveCheck = new QCheckBox("");
    
    QFormLayout* formLayout = new QFormLayout();
    formLayout->addRow("Server", serverEdit);
    formLayout->addRow("User", userEdit);
    formLayout->addRow("Password", passwordEdit);
    formLayout->addRow("Stay logged in", saveCheck);
    
    QVBoxLayout* mainLayout = new QVBoxLayout();
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(loginButton);
    mainLayout->addWidget(sessionLabel);
    
    setLayout(mainLayout);

    settings = new QSettings(QString("Quaternion"));
    settings->beginGroup("Login");
    serverEdit->setText(settings->value("homeserver", "https://matrix.org").toString());
    saveCheck->setChecked(settings->value("savecredentials", false).toBool());
    settings->endGroup();

    
    connect( loginButton, &QPushButton::clicked, this, &LoginDialog::login );
}

QuaternionConnection* LoginDialog::connection() const
{
    return m_connection;
}

void LoginDialog::login()
{
    qDebug() << "login";
    setDisabled(true);
    QUrl url = QUrl::fromUserInput(serverEdit->text());
    QString user = userEdit->text();
    QString password = passwordEdit->text();

    setConnection(new QuaternionConnection(url));

    settings->beginGroup("Login");
    settings->setValue("savecredentials", saveCheck->isChecked());
    if(saveCheck->isChecked()){
        settings->setValue("homeserver", serverEdit->text());
    }
    settings->endGroup();

    connect( m_connection, &QMatrixClient::Connection::connected, this, &QDialog::accept );
    connect( m_connection, &QMatrixClient::Connection::loginError, this, &LoginDialog::error );
    m_connection->connectToServer(user, password);
}

void LoginDialog::error(QString error)
{
    sessionLabel->setText( error );
    setDisabled(false);
}

void LoginDialog::setDisabled(bool state) {
    QDialog::setDisabled(state);
    serverEdit->setDisabled(state);
    userEdit->setDisabled(state);
    loginButton->setDisabled(state);
}

void LoginDialog::setConnection(QuaternionConnection* connection) {
    emit connectionChanged(connection);

    if (m_connection != nullptr) {
        delete m_connection;
    }

    m_connection = connection;
}
