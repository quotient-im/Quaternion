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

#include "joinroomjob.h"

#include <QtCore/QJsonObject>
#include <QtNetwork/QNetworkReply>

#include "../connectiondata.h"

using namespace QMatrixClient;

class JoinRoomJob::Private
{
    public:
        QString roomId;
        QString roomAlias;
        QNetworkReply* reply;
};

JoinRoomJob::JoinRoomJob(ConnectionData* data, QString roomAlias)
    : BaseJob(data)
    , d(new Private)
{
    d->roomAlias = roomAlias;
}

JoinRoomJob::~JoinRoomJob()
{
    delete d;
}

void JoinRoomJob::start()
{
    QString path = QString("_matrix/client/api/v1/join/%1").arg(d->roomAlias);
    QUrlQuery query;
    query.addQueryItem("access_token", connection()->token());
    QJsonObject json;
    d->reply = post(path, QJsonDocument(json), query );
    connect( d->reply, &QNetworkReply::finished, this, &JoinRoomJob::gotReply );
}

QString JoinRoomJob::roomId()
{
    return d->roomId;
}

void JoinRoomJob::gotReply()
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
    if( !json.contains("room_id") )
    {
        fail( KJob::UserDefinedError+2, "Something went wrong..." );
        qDebug() << data;
        return;
    }
    else
    {
        d->roomId = json.value("room_id").toString();
    }
    emitResult();
}