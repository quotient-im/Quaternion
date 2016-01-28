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

#include "roomcanonicalaliasevent.h"

using namespace QMatrixClient;

class RoomCanonicalAliasEvent::Private
{
    public:
        QString alias;
};

RoomCanonicalAliasEvent::RoomCanonicalAliasEvent()
    : Event(EventType::RoomCanonicalAlias)
    , d(new Private)
{
}

RoomCanonicalAliasEvent::~RoomCanonicalAliasEvent()
{
    delete d;
}

QString RoomCanonicalAliasEvent::alias()
{
    return d->alias;
}

RoomCanonicalAliasEvent* RoomCanonicalAliasEvent::fromJson(const QJsonObject& obj)
{
    RoomCanonicalAliasEvent* e = new RoomCanonicalAliasEvent();
    e->parseJson(obj);
    const QJsonObject contents = obj.value("content").toObject();
    e->d->alias = contents.value("alias").toString();
    return e;
}

