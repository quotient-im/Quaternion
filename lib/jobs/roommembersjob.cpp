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

#include "roommembersjob.h"

#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

#include "../room.h"
#include "../state.h"

using namespace QMatrixClient;

class RoomMembersJob::Private
{
    public:
        Room* room;
        QList<State*> states;
};

RoomMembersJob::RoomMembersJob(ConnectionData* data, Room* room)
    : BaseJob(data, JobHttpType::GetJob)
    , d(new Private)
{
    d->room = room;
}

RoomMembersJob::~RoomMembersJob()
{
    delete d;
}

QList< State* > RoomMembersJob::states()
{
    return d->states;
}

QString RoomMembersJob::apiPath()
{
    return QString("_matrix/client/r0/rooms/%1/members").arg(d->room->id());
}

void RoomMembersJob::parseJson(const QJsonDocument& data)
{
    QJsonObject obj = data.object();
    QJsonArray chunk = obj.value("chunk").toArray();
    for( const QJsonValue& val : chunk )
    {
        State* state = State::fromJson(val.toObject());
        if( state )
            d->states.append(state);
    }
    qDebug() << "States: " << d->states.count();
    emitResult();
}