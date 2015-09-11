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

#include "geteventsjob.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QJsonArray>
#include <QtCore/QDebug>

#include <QtNetwork/QNetworkReply>

#include "../room.h"
#include "../connectiondata.h"
#include "../events/event.h"

using namespace QMatrixClient;

class GetEventsJob::Private
{
    public:
        Private() {reply=0;}

        QList<Event*> events;
        QString from;
        QNetworkReply* reply;
};

GetEventsJob::GetEventsJob(ConnectionData* connection, QString from)
    : BaseJob(connection)
    , d(new Private)
{
    if( from.isEmpty() )
        from = connection->lastEvent();
    d->from = from;
}

GetEventsJob::~GetEventsJob()
{
    delete d;
}

void GetEventsJob::start()
{
    QString path = "_matrix/client/api/v1/events";
    QUrlQuery query;
    query.addQueryItem("access_token", connection()->token());
    query.addQueryItem("from", d->from);
    //query.addQueryItem("timeout", "10000");
    d->reply = get(path, query);
    connect( d->reply, &QNetworkReply::finished, this, &GetEventsJob::gotReply );
}

QList< Event* > GetEventsJob::events()
{
    return d->events;
}

void GetEventsJob::gotReply()
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
    if( !json.contains("chunk") || !json.value("chunk").isArray() )
    {
        fail( KJob::UserDefinedError+2, "Couldn't find chunk" );
        return;
    }
    QJsonArray chunk = json.value("chunk").toArray();
    qDebug() << chunk;
    for( const QJsonValue& val: chunk )
    {
        QJsonObject eventObj = val.toObject();
        Event* event = Event::fromJson(eventObj);
        if( event )
        {
            d->events.append(event);
        }
    }
    connection()->setLastEvent( json.value("end").toString() );
    emitResult();
}