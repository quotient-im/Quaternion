/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2019 Karol Kosek <krkkx@protonmail.com>        *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#pragma once

#include "dialog.h"

#include <settings.h>
#include <csapi/definitions/client_device.h>

#include <QtCore/QPointer>

class AccountSelector;
class MainWindow;

class QComboBox;
class QLineEdit;

namespace Quotient {
class AccountRegistry;
class GetDevicesJob;
class Connection;
}

class ProfileDialog : public Dialog
{
    Q_OBJECT
public:
    explicit ProfileDialog(Quotient::AccountRegistry* accounts,
                           MainWindow* parent);
    ~ProfileDialog() override;

    void setAccount(Quotient::Connection* newAccount);
    Quotient::Connection* account() const;

private slots:
    void load() override;
    void apply() override;
    void uploadAvatar();

private:
    Quotient::SettingsGroup m_settings;

    class DeviceTable;
    DeviceTable* m_deviceTable;
    QPushButton* m_avatar;
    AccountSelector* m_accountSelector;
    QLineEdit* m_displayName;
    QLabel* m_accessTokenLabel;

    Quotient::Connection* m_currentAccount;
    QString m_newAvatarPath;
    QPointer<Quotient::GetDevicesJob> m_devicesJob;
    QVector<Quotient::Device> m_devices;
};
