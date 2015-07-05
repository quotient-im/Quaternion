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

#include "mainwindow.h"

#include <QtCore/QTimer>
#include <QtCore/QDebug>

#include "roomlistdock.h"
#include "chatroomwidget.h"
#include "logindialog.h"
#include "lib/jobs/initialsyncjob.h"
#include "lib/jobs/geteventsjob.h"

MainWindow::MainWindow()
{
    connection = 0;
    roomMap = 0;
    roomListDock = new RoomListDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, roomListDock);
    chatRoomWidget = new ChatRoomWidget(this);
    setCentralWidget(chatRoomWidget);
    connect( roomListDock, &RoomListDock::roomSelected, chatRoomWidget, &ChatRoomWidget::setRoom );
    show();
    QTimer::singleShot(0, this, &MainWindow::initialize);
}

MainWindow::~MainWindow()
{
}

void MainWindow::initialize()
{
    LoginDialog dialog(this);
    if( dialog.exec() )
    {
        connection = dialog.connection();
        QMatrixClient::InitialSyncJob* job = new QMatrixClient::InitialSyncJob(connection);
        connect( job, &QMatrixClient::InitialSyncJob::result, this, &MainWindow::initialSync );
        job->start();
    }
}

void MainWindow::initialSync(KJob* job)
{
    qDebug() << "initial";
    QMatrixClient::InitialSyncJob* realJob = static_cast<QMatrixClient::InitialSyncJob*>(job);
    if( realJob->error() )
    {
        qDebug() << realJob->errorText();
        return;
    }
    qDebug() << "blub";
    roomMap = realJob->roomMap();
    roomListDock->setRoomMap( roomMap );
    //chatRoomWidget->setRoom( roomMap->values().first() );
    QTimer::singleShot(0, this, &MainWindow::getNewEvents);
}

void MainWindow::getNewEvents()
{
    qDebug() << "getNewEvents";
    qDebug() << "Keys:" << roomMap->keys();
    QMatrixClient::GetEventsJob* job = new QMatrixClient::GetEventsJob(connection, roomMap);
    connect( job, &QMatrixClient::GetEventsJob::result, this, &MainWindow::newEvents );
    job->start();
}

void MainWindow::newEvents(KJob* job)
{
    qDebug() << "newEvents";
    QMatrixClient::GetEventsJob* realJob = static_cast<QMatrixClient::GetEventsJob*>(job);
    // TODO: Add new rooms
    getNewEvents();
}


