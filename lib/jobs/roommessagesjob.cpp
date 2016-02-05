/******************************************************************************
 * Copyright (C) 2016 Felix Rohrbach <kde@fxrh.de>
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

#include "roommessagesjob.h"
#include "../room.h"
#include "../events/event.h"

#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

using namespace QMatrixClient;

class RoomMessagesJob::Private
{
    public:
        Private() {}

        Room* room;
        QString from;
        FetchDirectory dir;
        int limit;

        QList<Event*> events;
        QString end;
};

RoomMessagesJob::RoomMessagesJob(ConnectionData* data, Room* room, QString from, FetchDirectory dir, int limit)
    : BaseJob(data, JobHttpType::GetJob)
{
    d = new Private();
    d->room = room;
    d->from = from;
    d->dir = dir;
    d->limit = limit;
}

RoomMessagesJob::~RoomMessagesJob()
{
    delete d;
}

QList<Event*> RoomMessagesJob::events()
{
    return d->events;
}

QString RoomMessagesJob::end()
{
    return d->end;
}

QString RoomMessagesJob::apiPath()
{
    return QString("/_matrix/client/r0/rooms/%1/messages").arg(d->room->id());
}

QUrlQuery RoomMessagesJob::query()
{
    QUrlQuery query;
    query.addQueryItem("from", d->from);
    if( d->dir == FetchDirectory::Backwards )
        query.addQueryItem("dir", "b");
    else
        query.addQueryItem("dir", "f");
    query.addQueryItem("limit", QString::number(d->limit));
    return query;
}

void RoomMessagesJob::parseJson(const QJsonDocument& data)
{
    QJsonObject obj = data.object();
    QJsonArray chunk = obj.value("chunk").toArray();
    for( const QJsonValue& val: chunk )
    {
        Event* event = Event::fromJson(val.toObject());
        d->events.append(event);
    }
    d->end = obj.value("end").toString();
    emitResult();
}