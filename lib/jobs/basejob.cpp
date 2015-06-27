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

#include "basejob.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include "../connection.h"

using namespace QMatrixClient;

class BaseJob::Private
{
    public:
        Private(Connection* c) {connection=c;}
        
        Connection* connection;
};

BaseJob::BaseJob(Connection* connection)
    : d(new Private(connection))
{
}

BaseJob::~BaseJob()
{
    delete d;
}

Connection* BaseJob::connection() const
{
    return d->connection;
}

QNetworkReply* BaseJob::get(const QString& path) const
{
    QUrl url = d->connection->baseUrl();
    url.setPath( url.path() + "/" + path );
    QNetworkRequest req = QNetworkRequest(url);
    return d->connection->nam()->get(req);
}

QNetworkReply* BaseJob::put(const QString& path, const QJsonDocument& data) const
{
    QUrl url = d->connection->baseUrl();
    url.setPath( url.path() + "/" + path );
    QNetworkRequest req = QNetworkRequest(url);
    return d->connection->nam()->put(req, data.toJson());
}

QNetworkReply* BaseJob::post(const QString& path, const QJsonDocument& data) const
{
    QUrl url = d->connection->baseUrl();
    url.setPath( url.path() + "/" + path );
    QNetworkRequest req = QNetworkRequest(url);
    return d->connection->nam()->post(req, data.toJson());
}

void BaseJob::fail(int errorCode, QString errorString)
{
    setError( errorCode );
    setErrorText( errorString );
    emitResult();
}