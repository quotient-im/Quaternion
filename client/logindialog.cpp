/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2015 Felix Rohrbach <kde@fxrh.de>              *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#include "logindialog.h"

#include "logging_categories.h"

#include <Quotient/connection.h>
#include <Quotient/accountregistry.h>
#include <Quotient/qt_connection_util.h>
#include <Quotient/ssosession.h>
#include <Quotient/settings.h>

#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QMessageBox>
#include <QtGui/QDesktopServices>

using Quotient::Connection;
using namespace Qt::StringLiterals;

static const auto MalformedServerUrl =
        LoginDialog::tr("The server URL doesn't look valid");

LoginDialog::LoginDialog(const QString& statusMessage, Quotient::AccountRegistry* loggedInAccounts,
                         QWidget* parent, const QStringList& knownAccounts)
    : Dialog(tr("Login"), parent, Dialog::StatusLine, tr("Login"), Dialog::NoExtraButtons)
    , userEdit(new QLineEdit(this))
    , passwordEdit(new QLineEdit(this))
    , initialDeviceName(new QLineEdit(this))
    , deviceId(new QLineEdit(this))
    , serverEdit(new QLineEdit(QStringLiteral("https://matrix.org"), this))
    , saveTokenCheck(new QCheckBox(tr("Stay logged in"), this))
    , m_connection(new Connection)
{
    setup(statusMessage);
    setPendingApplyMessage(tr("Connecting and logging in, please wait"));

    connect(userEdit, &QLineEdit::editingFinished, m_connection.get(),
            [this, loggedInAccounts, knownAccounts] {
                auto userId = userEdit->text();
                if (!userId.startsWith('@') || !userId.contains(':'))
                    return;

                button(QDialogButtonBox::Ok)->setEnabled(false);
                if (loggedInAccounts->get(userId)) {
                    setStatusMessage(
                        tr("This account is logged in already"));
                    return;
                }
                if (knownAccounts.contains(userId)) {
                    Quotient::AccountSettings acct{ userId };
                    initialDeviceName->setText(acct.deviceName());
                    deviceId->setText(acct.deviceId());
                    saveTokenCheck->setChecked(acct.keepLoggedIn());
                } else {
                    initialDeviceName->clear();
                    deviceId->clear();
                    saveTokenCheck->setChecked(true);
                }
                setStatusMessage(tr("Resolving the homeserver..."));
                serverEdit->clear();
                m_connection->resolveServer(userId);
            });

    connect(serverEdit, &QLineEdit::editingFinished, m_connection.get(),
            [this, knownAccounts] {
                if (QUrl hsUrl{ serverEdit->text() }; hsUrl.isValid()) {
                    m_connection->setHomeserver(hsUrl);
                    button(QDialogButtonBox::Ok)->setEnabled(true);
                } else {
                    setStatusMessage(MalformedServerUrl);
                    button(QDialogButtonBox::Ok)->setEnabled(false);
                }
            });

    // This button is only shown when BOTH password auth and SSO are available
    // If only one flow is there, the "Login" button text is changed instead
    auto* ssoButton = buttonBox()->addButton(tr("Login with SSO"), QDialogButtonBox::AcceptRole);
    connect(ssoButton, &QPushButton::clicked, this, &LoginDialog::loginWithSso);
    ssoButton->setHidden(true);
    connect(m_connection.get(), &Connection::loginFlowsChanged, this,
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

    // Fill defaults
    if (!knownAccounts.empty()) {
        Quotient::AccountSettings account { knownAccounts.front() };
        userEdit->setText(account.userId());

        auto homeserver = account.homeserver();
        if (!homeserver.isEmpty())
            m_connection->setHomeserver(homeserver);

        initialDeviceName->setText(account.deviceName());
        deviceId->setText(account.deviceId());
        saveTokenCheck->setChecked(account.keepLoggedIn());
    } else {
        saveTokenCheck->setChecked(true);
    }
}

LoginDialog::LoginDialog(const QString& statusMessage,
                         const Quotient::AccountSettings& reloginAccount, QWidget* parent)
    : Dialog(tr("Re-login"), parent, Dialog::StatusLine, tr("Re-login"), Dialog::NoExtraButtons)
    , userEdit(new QLineEdit(reloginAccount.userId(), this))
    , passwordEdit(new QLineEdit(this))
    , initialDeviceName(new QLineEdit(reloginAccount.deviceName(), this))
    , deviceId(new QLineEdit(reloginAccount.deviceId(), this))
    , serverEdit(new QLineEdit(reloginAccount.homeserver().toString(), this))
    , saveTokenCheck(new QCheckBox(tr("Stay logged in"), this))
    , m_connection(new Connection)
{
    setup(statusMessage);
    userEdit->setReadOnly(true);
    userEdit->setFrame(false);
    initialDeviceName->setReadOnly(true);
    initialDeviceName->setFrame(false);
    saveTokenCheck->setEnabled(reloginAccount.keepLoggedIn());
    setPendingApplyMessage(tr("Restoring access, please wait"));
}

void LoginDialog::setup(const QString& statusMessage)
{
    setStatusMessage(statusMessage);
    passwordEdit->setEchoMode( QLineEdit::Password );

    // This is triggered whenever the server URL has been changed
    connect(m_connection.get(), &Connection::homeserverChanged, serverEdit,
            [this](const QUrl& hsUrl) {
        serverEdit->setText(hsUrl.toString());
        if (hsUrl.isValid())
            setStatusMessage(tr("Getting supported login flows..."));

        // Allow to click login even before getting the flows and
        // do LoginDialog::loginWithBestFlow() as soon as flows arrive
        button(QDialogButtonBox::Ok)->setEnabled(hsUrl.isValid());
    });
    connect(m_connection.get(), &Connection::loginFlowsChanged, this, [this] {
        serverEdit->setText(m_connection->homeserver().toString());
        setStatusMessage(!m_connection->loginFlows().empty()
                             ? tr("The homeserver is available")
                             : tr("Could not connect to the homeserver"));
        button(QDialogButtonBox::Ok)->setEnabled(!m_connection->loginFlows().isEmpty());
        passwordEdit->setEnabled(m_connection->supportsPasswordAuth());
    });
    // This overrides the above in case of an unsuccessful attempt to resolve
    // the server URL from a changed MXID
    connect(m_connection.get(), &Connection::resolveError, this,
            [this](const QString& message) {
                qCDebug(MAIN) << "Failed to resolve the homeserver:" << message;
                serverEdit->clear();
                setStatusMessage(message);
            });
    deviceId->setReadOnly(true);
    deviceId->setFrame(false);
    deviceId->setPlaceholderText(tr(
        "(none)", "The device id label text when there's no saved device id"));
    connect(initialDeviceName, &QLineEdit::textChanged, deviceId, &QLineEdit::clear);

    connect(m_connection.get(), &Connection::connected, this, &Dialog::accept);
    connect(m_connection.get(), &Connection::loginError, this, &Dialog::applyFailed);

    // Lay out controls on the dialog

    auto* mainLayout = addLayout<QFormLayout>();

    mainLayout->addRow(tr("Matrix ID"), userEdit);
    auto* homeserverLayout = new QFormLayout();
    homeserverLayout->addRow(tr("Connect to server"), serverEdit);
    mainLayout->addRow({}, homeserverLayout);

    mainLayout->addRow(tr("Device name"), initialDeviceName);
    auto* deviceIdLayout = new QFormLayout();
    deviceIdLayout->addRow(tr("Saved device id"), deviceId);
    mainLayout->addRow({}, deviceIdLayout);

    mainLayout->addRow(tr("Password"), passwordEdit);
    mainLayout->addRow(saveTokenCheck);

    setTabOrder({ userEdit, initialDeviceName, passwordEdit, saveTokenCheck, serverEdit, deviceId });
    userEdit->setFocus();
}

Connection* LoginDialog::releaseConnection()
{
    return m_connection.release();
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
        m_connection->setHomeserver(url).then([this](auto) {
            qCDebug(MAIN) << "Received login flows, trying to login";
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
                                    initialDeviceName->text(), deviceId->text());
}

void LoginDialog::loginWithSso()
{
    auto* ssoSession = m_connection->prepareForSso(initialDeviceName->text(),
                                                   deviceId->text());
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
