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

#include "roommessageevent.h"

#include <QtCore/QJsonObject>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>

using namespace QMatrixClient;

class RoomMessageEvent::Private
{
    public:
        Private() {}
        
        QString userId;
        QString roomId;
        QString body;
        QString msgtype;
        QDateTime hsob_ts;
};

RoomMessageEvent::RoomMessageEvent()
    : Event(EventType::RoomMessage)
    , d(new Private)
{

}

QString RoomMessageEvent::userId() const
{
    return d->userId;
}

QString RoomMessageEvent::roomId() const
{
    return d->roomId;
}

QString RoomMessageEvent::msgtype() const
{
    return d->msgtype;
}

QString RoomMessageEvent::body() const
{
    return d->body;
}

QDateTime RoomMessageEvent::hsob_ts() const
{
    return d->hsob_ts;
}

RoomMessageEvent* RoomMessageEvent::fromJsonObject(const QJsonObject& obj)
{
    RoomMessageEvent* e = new RoomMessageEvent();
    e->parseJson(obj);
    if( obj.contains("user_id") )
    {
        e->d->userId = obj.value("user_id").toString();
    } else {
        qDebug() << "RoomMessageEvent: user_id not found";
    }
    if( obj.contains("room_id") )
    {
        e->d->roomId = obj.value("room_id").toString();
    } else {
        qDebug() << "RoomMessageEvent: room_id not found";
    }
    if( obj.contains("content") )
    {
        QJsonObject content = obj.value("content").toObject();
        if( content.contains("msgtype") )
        {
            e->d->msgtype = content.value("msgtype").toString();
        } else {
            qDebug() << "RoomMessageEvent: msgtype not found";
        }
        if( content.contains("body") )
        {
            e->d->hsob_ts = QDateTime::fromMSecsSinceEpoch( content.value("hsoc_ts").toInt() );
        } else {
            qDebug() << "RoomMessageEvent: hsoc_ts not found";
        }
    }
    return e;
}