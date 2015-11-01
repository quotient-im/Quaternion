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

#include "connection.h"
#include "connectiondata.h"
#include "connectionprivate.h"
#include "jobs/passwordlogin.h"
#include "jobs/initialsyncjob.h"
#include "jobs/geteventsjob.h"
#include "jobs/postmessagejob.h"
#include "jobs/joinroomjob.h"
#include "jobs/leaveroomjob.h"

#include <QtCore/QDebug>

using namespace QMatrixClient;

Connection::Connection(QUrl server, QObject* parent)
    : QObject(parent)
{
    d = new ConnectionPrivate(this);
    d->data = new ConnectionData(server);
}

Connection::~Connection()
{
    delete d;
}

void Connection::connectToServer(QString user, QString password)
{
    PasswordLogin* loginJob = new PasswordLogin(d->data, user, password);
    connect( loginJob, &PasswordLogin::result, d, &ConnectionPrivate::connectDone );
    loginJob->start();
}

void Connection::startInitialSync()
{
    InitialSyncJob* syncJob = new InitialSyncJob(d->data);
    connect( syncJob, &InitialSyncJob::result, d, &ConnectionPrivate::initialSyncDone );
    syncJob->start();
}

void Connection::getEvents()
{
    GetEventsJob* job = new GetEventsJob(d->data);
    connect( job, &GetEventsJob::result, d, &ConnectionPrivate::gotEvents );
    job->start();
}

void Connection::postMessage(Room* room, QString type, QString message)
{
    PostMessageJob* job = new PostMessageJob(d->data, room, type, message);
    job->start();
}

void Connection::joinRoom(QString roomAlias)
{
    JoinRoomJob* job = new JoinRoomJob(d->data, roomAlias);
    connect( job, &JoinRoomJob::result, d, &ConnectionPrivate::gotJoinRoom );
    job->start();
}

void Connection::leaveRoom(Room* room)
{
    LeaveRoomJob* job = new LeaveRoomJob(d->data, room);
    job->start();
}

QHash< QString, Room* > Connection::roomMap() const
{
    return d->roomMap;
}

bool Connection::isConnected()
{
    return d->isConnected;
}