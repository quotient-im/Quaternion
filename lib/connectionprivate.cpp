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
#include "jobs/passwordlogin.h"
#include "jobs/initialsyncjob.h"
#include "jobs/geteventsjob.h"
#include "events/event.h"
#include "events/roommessageevent.h"

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

void ConnectionPrivate::processEvent(Event* event)
{
    if( !event->roomId().isEmpty() )
    {
        Room* room;
        if( !roomMap.contains(event->roomId()) )
        {
            room = new Room(q, event->roomId());
            roomMap.insert( event->roomId(), room );
            emit q->newRoom(room);
        } else {
            room = roomMap.value(event->roomId());
        }
        room->addMessage(event);
    }
}

void ConnectionPrivate::processState(State* state)
{
    QString roomId = state->event()->roomId();
    if( !roomId.isEmpty() )
    {
        Room* room;
        if( !roomMap.contains(roomId) )
        {
            room = new Room(q, roomId);
            roomMap.insert( roomId, room );
            emit q->newRoom(room);
        } else {
            room = roomMap.value(roomId);
        }
        room->addInitialState(state);
    }
}

void ConnectionPrivate::connectDone(KJob* job)
{
    PasswordLogin* realJob = static_cast<PasswordLogin*>(job);
    if( !realJob->error() )
    {
        isConnected = true;
        emit q->connected();
    }
    else {
        emit q->loginError( job->errorString() );
    }
}

void ConnectionPrivate::initialSyncDone(KJob* job)
{
    InitialSyncJob* syncJob = static_cast<InitialSyncJob*>(job);
    if( !syncJob->error() )
    {
        QList<State*> initialState = syncJob->initialState();
        for( State* s: initialState )
            processState(s);
        QList<Event*> events = syncJob->events();
        for( Event* e: events )
            processEvent(e);
        emit q->initialSyncDone();
    }
    else
    {
        emit q->connectionError( syncJob->errorString() );
    }
}

void ConnectionPrivate::gotEvents(KJob* job)
{
    GetEventsJob* eventsJob = static_cast<GetEventsJob*>(job);
    if( !eventsJob->error() )
    {
        QList<Event*> events = eventsJob->events();
        for( Event* e: events )
        {
            processEvent(e);
        }
        emit q->gotEvents();
    }
    else
    {
        emit q->connectionError( eventsJob->errorString() );
    }
}