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

#include "syncjob.h"

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

class SyncJob::Private
{
    public:
        QString since;
        QString filter;
        bool fullState;
        QString presence;
        int timeout;
        QString nextBatch;

        QList<SyncRoomData> roomData;

        void parseEvents(QString roomId, const QJsonObject& room, JoinState joinState);
};

SyncJob::SyncJob(ConnectionData* connection, QString since)
    : BaseJob(connection, JobHttpType::GetJob)
    , d(new Private)
{
    d->since = since;
    d->fullState = false;
    d->timeout = -1;
}

SyncJob::~SyncJob()
{
    delete d;
}

void SyncJob::setFilter(QString filter)
{
    d->filter = filter;
}

void SyncJob::setFullState(bool full)
{
    d->fullState = full;
}

void SyncJob::setPresence(QString presence)
{
    d->presence = presence;
}

void SyncJob::setTimeout(int timeout)
{
    d->timeout = timeout;
}

QString SyncJob::nextBatch() const
{
    return d->nextBatch;
}

QList<SyncRoomData> SyncJob::roomData() const
{
    return d->roomData;
}

QString SyncJob::apiPath()
{
    return "_matrix/clients/r0/sync";
}

QUrlQuery SyncJob::query()
{
    QUrlQuery query;
    if( !d->filter.isEmpty() )
        query.addQueryItem("filter", d->filter);
    if( d->fullState )
        query.addQueryItem("full_state", "true");
    if( !d->presence.isEmpty() )
        query.addQueryItem("set_presence", d->presence);
    if( d->timeout >= 0 )
        query.addQueryItem("timeout", QString::number(d->timeout));
    if( !d->since.isEmpty() )
        query.addQueryItem("since", d->since);
    return query;
}

void SyncJob::parseJson(const QJsonDocument& data)
{
    QJsonObject json = data.object();
    d->nextBatch = json.value("next_batch").toString();
    // TODO: presence
    // TODO: account_data
    QJsonObject rooms = json.value("rooms").toObject();

    QJsonObject joinRooms = rooms.value("join").toObject();
    for( const QString& roomId: joinRooms.keys() )
    {
        d->parseEvents(roomId, joinRooms.value(roomId).toObject(), JoinState::Join);
    }

    QJsonObject inviteRooms = rooms.value("invite").toObject();
    for( const QString& roomId: inviteRooms.keys() )
    {
        d->parseEvents(roomId, inviteRooms.value(roomId).toObject(), JoinState::Invite);
    }

    QJsonObject leaveRooms = rooms.value("leave").toObject();
    for( const QString& roomId: leaveRooms.keys() )
    {
        d->parseEvents(roomId, leaveRooms.value(roomId).toObject(), JoinState::Leave);
    }
}

void SyncJob::Private::parseEvents(QString roomId, const QJsonObject& room, JoinState joinState)
{
    SyncRoomData data;
    data.roomId = roomId;
    data.joinState = joinState;

    QJsonArray stateArray = room.value("state").toObject().value("events").toArray();
    for( QJsonValue val: stateArray )
    {
        Event* event = Event::fromJson(val.toObject());
        if( event )
            data.state.append(event);
    }

    QJsonObject timeline = room.value("timeline").toObject();
    QJsonArray timelineArray = timeline.value("events").toArray();
    for( QJsonValue val: timelineArray )
    {
        Event* event = Event::fromJson(val.toObject());
        if( event )
            data.timeline.append(event);
    }
    data.timelineLimited = timeline.value("limited").toBool();
    data.timelinePrevBatch = timeline.value("prev_batch").toString();

    QJsonArray ephemeralArray = room.value("ephemeral").toObject().value("events").toArray();
    for( QJsonValue val: ephemeralArray )
    {
        Event* event = Event::fromJson(val.toObject());
        if( event )
            data.ephemeral.append(event);
    }

    QJsonArray accountDataArray = room.value("account_data").toObject().value("events").toArray();
    for( QJsonValue val: accountDataArray )
    {
        Event* event = Event::fromJson(val.toObject());
        if( event )
            data.accountData.append(event);
    }
    roomData.append(data);
}

