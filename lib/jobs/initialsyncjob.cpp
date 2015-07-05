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

#include "../connection.h"
#include "../room.h"
#include "../logmessage.h"

using namespace QMatrixClient;

class InitialSyncJob::Private
{
    public:
        Private() {reply=0;roomMap=0;}

        void parseRoomMessage(const QJsonObject& obj, QHash<QString, QMatrixClient::Room*>* map);

        QNetworkReply* reply;
        QHash<QString, QMatrixClient::Room*>* roomMap;
};

void InitialSyncJob::Private::parseRoomMessage(const QJsonObject& obj, QHash< QString, QMatrixClient::Room* >* map)
{
    QString roomId = obj.value("room_id").toString();
    if( !map->contains(roomId) )
    {
        map->insert(roomId, new QMatrixClient::Room(roomId));
    }
    QMatrixClient::LogMessage* msg = new QMatrixClient::LogMessage( QMatrixClient::LogMessage::UserMessage,
                                                                    obj.value("content").toObject().value("body").toString(),
                                                                    obj.value("user_id").toString() );
    map->value(roomId)->addMessage( msg );
}

InitialSyncJob::InitialSyncJob(Connection* connection)
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

QHash< QString, Room* >* InitialSyncJob::roomMap()
{
    return d->roomMap;
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
    d->roomMap = new QHash<QString, QMatrixClient::Room*>();
    for( const QJsonValue& val : array )
    {
        if( !val.isObject() )
        {
            qWarning() << "Strange: " << val;
            continue;
        }
        QJsonObject obj = val.toObject();
        QString id = obj.value("room_id").toString();
        Room* room = new Room(id);
        room->parseEvents(obj);
        d->roomMap->insert(id, room);
//         qDebug() << obj.value("type");
//         if( obj.value("type") == "m.room.message" )
//         {
//             d->parseRoomMessage(obj, d->roomMap);
//         }
    }
    connection()->setLastEvent( json.value("end").toString() );
    qDebug() << connection()->lastEvent();
    emitResult();
}
