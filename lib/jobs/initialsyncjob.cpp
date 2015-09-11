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
        Private() {reply=0;}

        QNetworkReply* reply;
        QList<Event*> events;
        QList<State*> initialState;
};

InitialSyncJob::InitialSyncJob(ConnectionData* connection)
    : BaseJob(connection)
    , d(new Private)
{
}

InitialSyncJob::~InitialSyncJob()
{
    delete d->reply;
    delete d;
}

void InitialSyncJob::start()
{
    QString path = "_matrix/client/api/v1/initialSync";
    QUrlQuery query;
    query.addQueryItem("access_token", connection()->token());
    query.addQueryItem("limit", "200");
    d->reply = get(path, query);
    connect( d->reply, &QNetworkReply::finished, this, &InitialSyncJob::gotReply );
}

QList< Event* > InitialSyncJob::events()
{
    return d->events;
}

QList< State* > InitialSyncJob::initialState()
{
    return d->initialState;
}

void InitialSyncJob::gotReply()
{
    if( d->reply->error() != QNetworkReply::NoError )
    {
        fail( KJob::UserDefinedError, d->reply->errorString() );
        return;
    }
    QJsonParseError error;
    QJsonDocument data = QJsonDocument::fromJson(d->reply->readAll(), &error);
    if( error.error != QJsonParseError::NoError )
    {
        fail( KJob::UserDefinedError+1, error.errorString() );
        return;
    }
    //qDebug() << data;
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
            qDebug() << val.toObject();
            State* state = State::fromJson(val.toObject());
            if( state )
                d->initialState.append( state );
        }
    }
    connection()->setLastEvent( json.value("end").toString() );
    qDebug() << connection()->lastEvent();
    emitResult();
}
