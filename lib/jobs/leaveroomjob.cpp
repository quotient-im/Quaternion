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

#include "leaveroomjob.h"

#include <QtNetwork/QNetworkReply>

#include "../room.h"
#include "../connectiondata.h"

using namespace QMatrixClient;

class LeaveRoomJob::Private
{
    public:
        Private(Room* r) : room(r) {}

        QNetworkReply* reply;
        Room* room;
};

LeaveRoomJob::LeaveRoomJob(ConnectionData* data, Room* room)
    : BaseJob(data)
    , d(new Private(room))
{
}

LeaveRoomJob::~LeaveRoomJob()
{
    delete d;
}

void LeaveRoomJob::start()
{
    QString path = QString("_matrix/client/api/v1/rooms/%1/leave").arg(d->room->id());
    QUrlQuery query;
    query.addQueryItem("access_token", connection()->token());
    QJsonObject json;
    d->reply = post(path, QJsonDocument(json), query );
    connect( d->reply, &QNetworkReply::finished, this, &LeaveRoomJob::gotReply );
}

void LeaveRoomJob::gotReply()
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
    emitResult();
}