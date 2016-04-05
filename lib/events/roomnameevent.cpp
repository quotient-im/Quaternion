/******************************************************************************
 * Copyright (C) 2015 Kitsune Ral <kitsune-ral@users.sf.net>
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

#include "roomnameevent.h"

using namespace QMatrixClient;

class RoomNameEvent::Private
{
    public:
        QString name;
};

RoomNameEvent::RoomNameEvent() :
    Event(EventType::RoomName),
    d(new Private)
{
}

RoomNameEvent::~RoomNameEvent()
{
    delete d;
}

QString RoomNameEvent::name() const
{
    return d->name;
}

RoomNameEvent* RoomNameEvent::fromJson(const QJsonObject& obj)
{
    RoomNameEvent* e = new RoomNameEvent();
    e->parseJson(obj);
    const QJsonObject contents = obj.value("content").toObject();
    e->d->name = contents.value("name").toString();
    return e;
}
