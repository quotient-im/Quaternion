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
#include "jobs/passwordlogin.h"
#include "jobs/initialsyncjob.h"
#include "jobs/geteventsjob.h"

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
        roomMap = syncJob->roomMap();
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
        QList<Room*> newRooms = eventsJob->newRooms();
        for( Room* r: newRooms )
        {
            emit q->newRoom( r );
        }
        emit q->gotEvents();
    }
    else
    {
        emit q->connectionError( eventsJob->errorString() );
    }
}