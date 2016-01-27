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

        QHash<QString, QJsonObject> joinedRooms;
        QHash<QString, QJsonObject> invitedRooms;
        QHash<QString, QJsonObject> leftRooms;
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

QHash<QString, QJsonObject> SyncJob::joinedRooms()
{
    return d->joinedRooms;
}

QHash<QString, QJsonObject> SyncJob::invitedRooms()
{
    return d->invitedRooms;
}

QHash<QString, QJsonObject> SyncJob::leftRooms()
{
    return d->leftRooms;
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
    QJsonObject rooms = json.value("rooms").toObject();

    QJsonObject joinRooms = rooms.value("join").toObject();
    for( const QString& roomId: joinRooms.keys() )
    {
        d->joinedRooms.insert(roomId, joinRooms.value(roomId).toObject());
    }

    QJsonObject inviteRooms = rooms.value("invite").toObject();
    for( const QString& roomId: inviteRooms.keys() )
    {
        d->invitedRooms.insert(roomId, inviteRooms.value(roomId).toObject());
    }

    QJsonObject leaveRooms = rooms.value("leave").toObject();
    for( const QString& roomId: leaveRooms.keys() )
    {
        d->leftRooms.insert(roomId, leaveRooms.value(roomId).toObject());
    }
}

