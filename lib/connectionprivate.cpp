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

#include "connectionprivate.h"
#include "connection.h"
#include "state.h"
#include "room.h"
#include "user.h"
#include "jobs/passwordlogin.h"
#include "jobs/syncjob.h"
#include "jobs/geteventsjob.h"
#include "jobs/joinroomjob.h"
#include "jobs/roommembersjob.h"
#include "events/event.h"
#include "events/roommessageevent.h"
#include "events/roommemberevent.h"

#include <QtCore/QDebug>

using namespace QMatrixClient;

ConnectionPrivate::ConnectionPrivate(Connection* parent)
    : q(parent)
{
    isConnected = false;
    data = 0;
}

ConnectionPrivate::~ConnectionPrivate()
{
    delete data;
}

void ConnectionPrivate::processState(State* state)
{
    QString roomId = state->event()->roomId();
    if( state->event()->type() == QMatrixClient::EventType::RoomMember )
    {
        QMatrixClient::RoomMemberEvent* e = static_cast<QMatrixClient::RoomMemberEvent*>(state->event());
        User* user = q->user(e->userId());
        user->processEvent(e);
    }
    if( !roomId.isEmpty() )
    {
        Room* room;
        if( !roomMap.contains(roomId) )
        {
            room = q->createRoom(roomId);
            roomMap.insert( roomId, room );
            emit q->newRoom(room);
        } else {
            room = roomMap.value(roomId);
        }
        room->addInitialState(state);
    }
}

void ConnectionPrivate::processRooms(const QList<SyncRoomData>& data)
{
    for( const SyncRoomData& roomData: data )
    {
        Room* room;
        if( !roomMap.contains(roomData.roomId) )
        {
            room = q->createRoom(roomData.roomId);
            roomMap.insert( roomData.roomId, room );
            emit q->newRoom(room);
        } else {
            room = roomMap.value(roomData.roomId);
        }
        room->updateData(roomData);
    }
}

void ConnectionPrivate::connectDone(KJob* job)
{
    PasswordLogin* realJob = static_cast<PasswordLogin*>(job);
    if( !realJob->error() )
    {
        isConnected = true;
        userId = realJob->id();
        qDebug() << "Our user ID: " << userId;
        emit q->connected();
    }
    else {
        emit q->loginError( job->errorString() );
    }
}

void ConnectionPrivate::reconnectDone(KJob* job)
{
    PasswordLogin* realJob = static_cast<PasswordLogin*>(job);
    if( !realJob->error() )
    {
        userId = realJob->id();
        emit q->reconnected();
    }
    else {
        emit q->loginError( job->errorString() );
        isConnected = false;
    }
}

void ConnectionPrivate::syncDone(KJob* job)
{
    SyncJob* syncJob = static_cast<SyncJob*>(job);
    if( !syncJob->error() )
    {
        data->setLastEvent(syncJob->nextBatch());
        processRooms(syncJob->roomData());
        emit q->syncDone();
    }
    else {
        if( syncJob->error() == BaseJob::NetworkError )
            emit q->connectionError( syncJob->errorString() );
        else
            qDebug() << "syncJob failed, error:" << syncJob->error();
    }
}

void ConnectionPrivate::gotJoinRoom(KJob* job)
{
    qDebug() << "gotJoinRoom";
    JoinRoomJob* joinJob = static_cast<JoinRoomJob*>(job);
    if( !joinJob->error() )
    {
        QString roomId = joinJob->roomId();
        Room* room;
        if( roomMap.contains(roomId) )
        {
            room = roomMap.value(roomId);
        } else {
            room = q->createRoom(roomId);
            roomMap.insert( roomId, room );
            emit q->newRoom(room);
        }
        emit q->joinedRoom(room);
    }
    else
    {
        if( joinJob->error() == BaseJob::NetworkError )
            emit q->connectionError( joinJob->errorString() );
    }
}

void ConnectionPrivate::gotRoomMembers(KJob* job)
{
    RoomMembersJob* membersJob = static_cast<RoomMembersJob*>(job);
    if( !membersJob->error() )
    {
        for( State* state: membersJob->states() )
        {
            processState(state);
        }
        qDebug() << membersJob->states().count() << " processed...";
    }
    else
    {
        qDebug() << "MembersJob error: " <<membersJob->errorString();
        if( membersJob->error() == BaseJob::NetworkError )
            emit q->connectionError( membersJob->errorString() );
    }
}