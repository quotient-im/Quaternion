/******************************************************************************
 * Copyright (C) 2015 Felix Rohrbach <kde@fxrh.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>

#include "lib/connection.h"
#include "lib/jobs/checkauthmethods.h"

class LoginDialog : public QDialog
{
        Q_OBJECT
    public:
        LoginDialog(QWidget* parent=0);
        
    private slots:
        void init();
        void initDone(KJob* job);
        
    private:
        QLineEdit* serverEdit;
        QPushButton* initButton;
        QLabel* sessionLabel;
        
        QMatrixClient::Connection* connection;
};

#endif // LOGINDIALOG_H