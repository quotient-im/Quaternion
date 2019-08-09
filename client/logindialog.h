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

#pragma once

#include "dialog.h"

class QLineEdit;
class QCheckBox;

namespace Quotient {
    class AccountSettings;
    class Connection;
}

class LoginDialog : public Dialog
{
        Q_OBJECT
    public:
        explicit LoginDialog(QWidget* parent = nullptr,
                             const QStringList& knownAccounts = {});
        explicit LoginDialog(QWidget* parent,
                             const Quotient::AccountSettings& reloginData);
        void setup();
        ~LoginDialog() override;

        Quotient::Connection* releaseConnection();
        QString deviceName() const;
        bool keepLoggedIn() const;

    private slots:
        void apply() override;
        
    private:
        QLineEdit* userEdit;
        QLineEdit* passwordEdit;
        QLineEdit* initialDeviceName;
        QLineEdit* serverEdit;
        QCheckBox* saveTokenCheck;

        QScopedPointer<Quotient::Connection, QScopedPointerDeleteLater>
            m_connection;
};
