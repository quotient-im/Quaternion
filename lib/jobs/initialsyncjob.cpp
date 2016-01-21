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

#include "initialsyncjob.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QUrlQuery>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonValue>

#include "../connectiondata.h"
#include "../room.h"
#include "../state.h"
#include "../events/event.h"

using namespace QMatrixClient;

class InitialSyncJob::Private
{
    public:
        Private() {}

        QList<Event*> events;
        QList<State*> initialState;
};

InitialSyncJob::InitialSyncJob(ConnectionData* connection)
    : BaseJob(connection, JobHttpType::GetJob)
    , d(new Private)
{
}

InitialSyncJob::~InitialSyncJob()
{
    delete d;
}

QList< Event* > InitialSyncJob::events()
{
    return d->events;
}

QList< State* > InitialSyncJob::initialState()
{
    return d->initialState;
}

QString InitialSyncJob::apiPath()
{
    return "_matrix/client/r0/initialSync";
}

QUrlQuery InitialSyncJob::query()
{
    QUrlQuery query;
    query.addQueryItem("limit", "200");
    return query;
}

void InitialSyncJob::parseJson(const QJsonDocument& data)
{
    QJsonObject json = data.object();
    if( !json.contains("rooms") || !json.value("rooms").isArray() )
    {
        fail( KJob::UserDefinedError+2, "Didn't find rooms" );
        qDebug() << json;
        return;
    }
    QJsonArray array = json.value("rooms").toArray();
    for( const QJsonValue& val : array )
    {
        if( !val.isObject() )
        {
            qWarning() << "Strange: " << val;
            continue;
        }
        QJsonObject obj = val.toObject();
        QJsonObject messages = obj.value("messages").toObject();
        QJsonArray chunk = messages.value("chunk").toArray();
        for( const QJsonValue& val: chunk )
        {
            Event* event = Event::fromJson(val.toObject());
            if( event )
                d->events.append( event );
        }
        QJsonArray state = obj.value("state").toArray();
        for( const QJsonValue& val: state )
        {
//             qDebug() << val.toObject();
            State* state = State::fromJson(val.toObject());
            if( state )
                d->initialState.append( state );
        }
    }
    connection()->setLastEvent( json.value("end").toString() );
    qDebug() << connection()->lastEvent();
    emitResult();
}

