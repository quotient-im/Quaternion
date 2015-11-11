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

#include "roomtopicevent.h"

using namespace QMatrixClient;

class RoomTopicEvent::Private
{
    public:
        QString topic;
};

RoomTopicEvent::RoomTopicEvent()
    : Event(EventType::RoomTopic)
    , d(new Private)
{
}

RoomTopicEvent::~RoomTopicEvent()
{
    delete d;
}

QString RoomTopicEvent::topic() const
{
    return d->topic;
}

RoomTopicEvent* RoomTopicEvent::fromJson(const QJsonObject& obj)
{
    RoomTopicEvent* e = new RoomTopicEvent();
    e->parseJson(obj);
    e->d->topic = obj.value("content").toObject().value("topic").toString();
    return e;
}
