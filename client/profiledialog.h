/**************************************************************************
 *                                                                        *
 * Copyright (C) 2019 Karol Kosek <krkkx@protonmail.com>                  *
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

#pragma once

#include "dialog.h"
#include "accountregistry.h"

#include <settings.h>
#include <csapi/definitions/client_device.h>

#include <QtCore/QPointer>

class AccountSelector;
class MainWindow;

class QComboBox;
class QLineEdit;

namespace Quotient {
    class GetDevicesJob;
    class Connection;
}

class ProfileDialog : public Dialog
{
    Q_OBJECT
public:
    explicit ProfileDialog(MainWindow* parent);
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
