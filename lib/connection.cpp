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
#include "user.h"
#include "events/event.h"
#include "room.h"
#include "jobs/passwordlogin.h"
#include "jobs/geteventsjob.h"
#include "jobs/postmessagejob.h"
#include "jobs/postreceiptjob.h"
#include "jobs/joinroomjob.h"
#include "jobs/leaveroomjob.h"
#include "jobs/roommembersjob.h"
#include "jobs/roommessagesjob.h"
#include "jobs/syncjob.h"
#include "jobs/mediathumbnailjob.h"

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
    d->username = user; // to be able to reconnect
    d->password = password;
}

void Connection::reconnect()
{
    PasswordLogin* loginJob = new PasswordLogin(d->data, d->username, d->password );
    connect( loginJob, &PasswordLogin::result, d, &ConnectionPrivate::reconnectDone );
    loginJob->start();
}

SyncJob* Connection::sync(int timeout)
{
    QString filter = "{\"room\": { \"timeline\": { \"limit\": 100 } } }";
    SyncJob* syncJob = new SyncJob(d->data, d->data->lastEvent());
    syncJob->setFilter(filter);
    syncJob->setTimeout(timeout);
    connect( syncJob, &SyncJob::result, d, &ConnectionPrivate::syncDone );
    syncJob->start();
    return syncJob;
}

void Connection::postMessage(Room* room, QString type, QString message)
{
    PostMessageJob* job = new PostMessageJob(d->data, room, type, message);
    job->start();
}

PostReceiptJob* Connection::postReceipt(Room* room, Event* event)
{
    PostReceiptJob* job = new PostReceiptJob(d->data, room->id(), event->id());
    job->start();
    return job;
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

void Connection::getMembers(Room* room)
{
    RoomMembersJob* job = new RoomMembersJob(d->data, room);
    connect( job, &RoomMembersJob::result, d, &ConnectionPrivate::gotRoomMembers );
    job->start();
}

RoomMessagesJob* Connection::getMessages(Room* room, QString from)
{
    RoomMessagesJob* job = new RoomMessagesJob(d->data, room, from);
    job->start();
    return job;
}

MediaThumbnailJob* Connection::getThumbnail(QUrl url, int requestedWidth, int requestedHeight)
{
    MediaThumbnailJob* job = new MediaThumbnailJob(d->data, url, requestedWidth, requestedHeight);
    job->start();
    return job;
}

User* Connection::user(QString userId)
{
    if( d->userMap.contains(userId) )
        return d->userMap.value(userId);
    User* user = createUser(userId);
    d->userMap.insert(userId, user);
    return user;
}

User *Connection::user()
{
    if( d->userId.isEmpty() )
        return 0;
    return user(d->userId);
}

QHash< QString, Room* > Connection::roomMap() const
{
    return d->roomMap;
}

bool Connection::isConnected()
{
    return d->isConnected;
}

ConnectionData* Connection::connectionData()
{
    return d->data;
}

User* Connection::createUser(QString userId)
{
    return new User(userId, this);
}

Room* Connection::createRoom(QString roomId)
{
    return new Room(this, roomId);
}
