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
#include <ssosession.h>
#include <settings.h>

#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QMessageBox>
#include <QtGui/QDesktopServices>

using Quotient::Connection;

static const auto MalformedServerUrl =
        LoginDialog::tr("The server URL doesn't look valid");

LoginDialog::LoginDialog(const QString& statusMessage, QWidget* parent,
                         const QStringList& knownAccounts)
    : Dialog(tr("Login"), parent, Dialog::StatusLine, tr("Login"),
             Dialog::NoExtraButtons)
    , userEdit(new QLineEdit(this))
    , passwordEdit(new QLineEdit(this))
    , initialDeviceName(new QLineEdit(this))
    , serverEdit(new QLineEdit(QStringLiteral("https://matrix.org"), this))
    , saveTokenCheck(new QCheckBox(tr("Stay logged in"), this))
    , m_connection(new Connection)
{
    setup(statusMessage);
    setPendingApplyMessage(tr("Connecting and logging in, please wait"));

    connect(userEdit, &QLineEdit::editingFinished, m_connection.data(), [this] {
        auto userId = userEdit->text();
        if (userId.startsWith('@') && userId.indexOf(':') != -1) {
            setStatusMessage(tr("Resolving the homeserver..."));
            serverEdit->clear();
            button(QDialogButtonBox::Ok)->setEnabled(false);
            m_connection->resolveServer(userId);
        }
    });

    connect(serverEdit, &QLineEdit::editingFinished, m_connection.data(), [this] {
        if (QUrl hsUrl { serverEdit->text() }; hsUrl.isValid()) {
            m_connection->setHomeserver(serverEdit->text());
            button(QDialogButtonBox::Ok)->setEnabled(true);
        } else {
            setStatusMessage(MalformedServerUrl);
            button(QDialogButtonBox::Ok)->setEnabled(false);
        }
    });

    // This button is only shown when BOTH password auth and SSO are available
    // If only one flow is there, the "Login" button text is changed instead
    auto* ssoButton = buttonBox()->addButton(tr("Login with SSO"),
                                             QDialogButtonBox::AcceptRole);
    connect(ssoButton, &QPushButton::clicked, this, &LoginDialog::loginWithSso);
    ssoButton->setHidden(true);
    connect(m_connection.data(), &Connection::loginFlowsChanged, this,
            [this, ssoButton] {
                // There may be more ways to login but Quaternion only supports
                // SSO and password for now; in the worst case of no known
                // options password login is kept enabled as the last resort.
                bool canUseSso = m_connection->supportsSso();
                bool canUsePassword = m_connection->supportsPasswordAuth();
                ssoButton->setVisible(canUseSso && canUsePassword);
                button(QDialogButtonBox::Ok)
                    ->setText(canUseSso && !canUsePassword
                                  ? QStringLiteral("Login with SSO")
                                  : QStringLiteral("Login"));
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

LoginDialog::LoginDialog(const QString &statusMessage, QWidget* parent,
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
    setup(statusMessage);
    userEdit->setReadOnly(true);
    userEdit->setFrame(false);
    setPendingApplyMessage(tr("Restoring access, please wait"));
}

void LoginDialog::setup(const QString& statusMessage)
{
    setStatusMessage(statusMessage);
    passwordEdit->setEchoMode( QLineEdit::Password );

    // This is triggered whenever the server URL has been changed
    connect(m_connection.data(), &Connection::homeserverChanged, serverEdit,
            [this](const QUrl& hsUrl) {
        serverEdit->setText(hsUrl.toString());
        if (hsUrl.isValid())
            setStatusMessage(tr("Getting supported login flows..."));

        // Allow to click login even before getting the flows and
        // do LoginDialog::loginWithBestFlow() as soon as flows arrive
        button(QDialogButtonBox::Ok)->setEnabled(hsUrl.isValid());
    });
    connect(m_connection.data(), &Connection::loginFlowsChanged, this, [this] {
        serverEdit->setText(m_connection->homeserver().toString());
        setStatusMessage(m_connection->isUsable()
                             ? tr("The homeserver is available")
                             : tr("Could not connect to the homeserver"));
        button(QDialogButtonBox::Ok)->setEnabled(m_connection->isUsable());
    });
    // This overrides the above in case of an unsuccessful attempt to resolve
    // the server URL from a changed MXID
    connect(m_connection.data(), &Connection::resolveError, this,
            [this](const QString& message) {
                qDebug() << "Resolve error";
                serverEdit->clear();
                setStatusMessage(message);
            });
    connect(m_connection.data(), &Connection::connected,
            this, &Dialog::accept);
    connect(m_connection.data(), &Connection::loginError,
            this, &Dialog::applyFailed);
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

    // Whichever the flow, the two connections are the same
    if (m_connection->homeserver() == url && !m_connection->loginFlows().empty())
        loginWithBestFlow();
    else if (!url.isValid())
        applyFailed(MalformedServerUrl);
    else {
        m_connection->setHomeserver(url);

        // Wait for new flows and check them
        connectSingleShot(m_connection.data(), &Connection::loginFlowsChanged,
                          this, [this] {
                              qDebug()
                                  << "Received login flows, trying to login";
                              loginWithBestFlow();
                          });
    }
}

void LoginDialog::loginWithBestFlow()
{
    if (m_connection->loginFlows().empty()
        || m_connection->supportsPasswordAuth())
        loginWithPassword();
    else if (m_connection->supportsSso())
        loginWithSso();
    else
        applyFailed(tr("No supported login flows"));
}

void LoginDialog::loginWithPassword()
{
    m_connection->loginWithPassword(userEdit->text(), passwordEdit->text(),
                                  initialDeviceName->text());
}

void LoginDialog::loginWithSso()
{
    auto* ssoSession = m_connection->prepareForSso(initialDeviceName->text());
    if (!QDesktopServices::openUrl(ssoSession->ssoUrl())) {
        auto* instructionsBox =
            new Dialog(tr("Single sign-on"), QDialogButtonBox::NoButton, this);
        instructionsBox->addWidget(new QLabel(
            tr("Quaternion couldn't automatically open the single sign-on URL. "
               "Please copy and paste it to the right application (usually "
               "a web browser):")));
        auto* urlBox = new QLineEdit(ssoSession->ssoUrl().toString());
        urlBox->setReadOnly(true);
        instructionsBox->addWidget(urlBox);
        instructionsBox->addWidget(
            new QLabel(tr("After authentication, the browser will follow "
                          "the temporary local address setup by Quaternion "
                          "to conclude the login sequence.")));
        instructionsBox->open();
    }
}
