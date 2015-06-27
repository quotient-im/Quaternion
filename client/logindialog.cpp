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

#include "logindialog.h"

#include <QtCore/QDebug>

LoginDialog::LoginDialog(QWidget* parent)
    : QDialog(parent)
{
    connection = 0;
    
    serverEdit = new QLineEdit();
    initButton = new QPushButton("Init");
    sessionLabel = new QLabel("Session:");
    
    QHBoxLayout* initLayout = new QHBoxLayout();
    initLayout->addWidget(serverEdit);
    initLayout->addWidget(initButton);
    
    QVBoxLayout* mainLayout = new QVBoxLayout();
    mainLayout->addLayout(initLayout);
    mainLayout->addWidget(sessionLabel);
    
    setLayout(mainLayout);
    
    connect( initButton, &QPushButton::clicked, this, &LoginDialog::init );
}

void LoginDialog::init()
{
    qDebug() << "init";
    QUrl url = QUrl::fromUserInput(serverEdit->text());
    connection = new QMatrixClient::Connection(url);
    QMatrixClient::CheckAuthMethods* job = new QMatrixClient::CheckAuthMethods(connection);
    connect( job, &QMatrixClient::CheckAuthMethods::result, this, &LoginDialog::initDone );
    job->start();
}

void LoginDialog::initDone(KJob* job)
{
    QMatrixClient::CheckAuthMethods* realJob = static_cast<QMatrixClient::CheckAuthMethods*>(job);
    if( realJob->error() )
    {
        sessionLabel->setText( realJob->errorText() );
    }
    else
    {
        sessionLabel->setText( "Session: " + realJob->session() );
    }
}
