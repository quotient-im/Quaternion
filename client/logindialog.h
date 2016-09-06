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

#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QtWidgets/QDialog>

class QLineEdit;
class QPushButton;
class QLabel;
class QCheckBox;
class QuaternionConnection;

class LoginDialog : public QDialog
{
        Q_OBJECT
    public:
        LoginDialog(QWidget* parent = nullptr);

        QuaternionConnection* connection() const;
        bool keepLoggedIn() const;

    private slots:
        void login();
        void error(QString error);
        
    private:
        QLineEdit* serverEdit;
        QLineEdit* userEdit;
        QLineEdit* passwordEdit;
        QPushButton* loginButton;
        QLabel* sessionLabel;
        QCheckBox* saveTokenCheck;
        
        QuaternionConnection* m_connection;

        void setConnection(QuaternionConnection* connection);
};

#endif // LOGINDIALOG_H
