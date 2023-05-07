/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2015 Felix Rohrbach <kde@fxrh.de>              *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#pragma once

#include "dialog.h"

#include <Quotient/connection.h>

class QLineEdit;
class QCheckBox;

namespace Quotient {
class AccountSettings;
class AccountRegistry;
}

#if QT_VERSION_MAJOR > 5
using DeleteLater = QScopedPointerDeleteLater;
#else
struct DeleteLater {
    void operator()(Quotient::Connection* ptr)
    {
        if (ptr)
            ptr->deleteLater();
    }
};
#endif

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

    std::unique_ptr<Quotient::Connection, DeleteLater> m_connection;
};
