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

#include "connectiondata.h"

#include <QtNetwork/QNetworkAccessManager>

using namespace QMatrixClient;

class ConnectionData::Private
{
    public:
        Private() {/*isConnected=false;*/}
        
        QUrl baseUrl;
        //bool isConnected;
        QString token;
        QString lastEvent;
        QNetworkAccessManager* nam;
};

ConnectionData::ConnectionData(QUrl baseUrl)
    : d(new Private)
{
    d->baseUrl = baseUrl;
    d->nam = new QNetworkAccessManager();
}

ConnectionData::~ConnectionData()
{
    d->nam->deleteLater();
    delete d;
}

// bool ConnectionData::isConnected() const
// {
//     return d->isConnected;
// }

QString ConnectionData::token() const
{
    return d->token;
}

QUrl ConnectionData::baseUrl() const
{
    return d->baseUrl;
}

QNetworkAccessManager* ConnectionData::nam() const
{
    return d->nam;
}

void ConnectionData::setToken(QString token)
{
    d->token = token;
}

QString ConnectionData::lastEvent() const
{
    return d->lastEvent;
}

void ConnectionData::setLastEvent(QString identifier)
{
    d->lastEvent = identifier;
}