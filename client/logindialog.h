/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2015 Felix Rohrbach <kde@fxrh.de>              *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#pragma once

#include "dialog.h"

class QLineEdit;
class QCheckBox;

namespace Quotient {
class AccountSettings;
class AccountRegistry;
class Connection;
}

class LoginDialog : public Dialog {
    Q_OBJECT
public:
    // FIXME: make loggedInAccounts pointer to const once we get to
    // libQuotient 0.8
    LoginDialog(const QString& statusMessage,
                Quotient::AccountRegistry* loggedInAccounts, QWidget* parent,
                const QStringList& knownAccounts = {});
    LoginDialog(const QString& statusMessage,
                const Quotient::AccountSettings& reloginAccount,
                QWidget* parent);
    void setup(const QString& statusMessage);
    ~LoginDialog() override;

    Quotient::Connection* releaseConnection();
    QString deviceName() const;
    bool keepLoggedIn() const;

private slots:
    void apply() override;
    void loginWithBestFlow();
    void loginWithPassword();
    void loginWithSso();

private:
    QLineEdit* userEdit;
    QLineEdit* passwordEdit;
    QLineEdit* initialDeviceName;
    QLineEdit* deviceId;
    QLineEdit* serverEdit;
    QCheckBox* saveTokenCheck;

    // TODO: Replace with plain QScopedPointerDeleteLater once we're on Qt 6
    using ConnectionDeleter = std::conditional_t<
        std::is_invocable_v<QScopedPointerDeleteLater, Quotient::Connection*>,
        QScopedPointerDeleteLater, decltype([](Quotient::Connection* ptr) {
            if (ptr)
                ptr->deleteLater();
        })>;

    std::unique_ptr<Quotient::Connection, ConnectionDeleter> m_connection;
};
