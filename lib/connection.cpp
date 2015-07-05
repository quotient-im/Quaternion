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

#include "connection.h"

#include <QtNetwork/QNetworkAccessManager>

using namespace QMatrixClient;

class Connection::Private
{
    public:
        Private() {isConnected=false;}
        
        QUrl baseUrl;
        bool isConnected;
        QString token;
        QString lastEvent;
        QNetworkAccessManager* nam;
};

Connection::Connection(QUrl baseUrl)
    : d(new Private)
{
    d->baseUrl = baseUrl;
    d->nam = new QNetworkAccessManager();
}

Connection::~Connection()
{
    d->nam->deleteLater();
    delete d;
}

bool Connection::isConnected() const
{
    return d->isConnected;
}

QString Connection::token() const
{
    return d->token;
}

QUrl Connection::baseUrl() const
{
    return d->baseUrl;
}

QNetworkAccessManager* Connection::nam() const
{
    return d->nam;
}

void Connection::setToken(QString token)
{
    d->token = token;
}

QString Connection::lastEvent() const
{
    return d->lastEvent;
}

void Connection::setLastEvent(QString identifier)
{
    d->lastEvent = identifier;
}