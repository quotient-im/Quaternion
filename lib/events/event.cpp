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

#include "event.h"

#include <QtCore/QJsonObject>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>

#include "roommessageevent.h"
#include "roomaliasesevent.h"
#include "roommemberevent.h"
#include "typingevent.h"
#include "unknownevent.h"

using namespace QMatrixClient;

class Event::Private
{
    public:
        EventType type;
        QString id;
        QDateTime timestamp;
        QString roomId;
};

Event::Event(EventType type)
    : d(new Private)
{
    d->type = type;
}

Event::~Event()
{
    delete d;
}

EventType Event::type() const
{
    return d->type;
}

QString Event::id() const
{
    return d->id;
}

QDateTime Event::timestamp() const
{
    return d->timestamp;
}

QString Event::roomId() const
{
    return d->roomId;
}

Event* Event::fromJson(const QJsonObject& obj)
{
    //qDebug() << obj.value("type").toString();
    if( obj.value("type").toString() == "m.room.message" )
    {
        qDebug() << "asd";
        return RoomMessageEvent::fromJson(obj);
    }
    if( obj.value("type").toString() == "m.room.aliases" )
    {
        return RoomAliasesEvent::fromJson(obj);
    }
    if( obj.value("type").toString() == "m.room.member" )
    {
        return RoomMemberEvent::fromJson(obj);
    }
    if( obj.value("type").toString() == "m.typing" )
    {
        qDebug() << "m.typing...";
        return TypingEvent::fromJson(obj);
    }
    //qDebug() << "Unknown event";
    if( obj.contains("room_id") )
    {
        return UnknownEvent::fromJson(obj);
    }
    return 0;
}

bool Event::parseJson(const QJsonObject& obj)
{
    bool correct = true;
    if( obj.contains("event_id") )
    {
        d->id = obj.value("event_id").toString();
    } else {
        correct = false;
        qDebug() << "Event: can't find event_id";
    }
    if( obj.contains("ts") )
    {
        d->timestamp = QDateTime::fromMSecsSinceEpoch( obj.value("ts").toInt() );
    } else {
        correct = false;
        qDebug() << "Event: can't find ts";
        //qDebug() << obj;
    }
    if( obj.contains("room_id") )
    {
        d->roomId = obj.value("room_id").toString();
    }
    return correct;
}
