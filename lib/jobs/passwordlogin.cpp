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

#include "passwordlogin.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtNetwork/QNetworkReply>

#include "../connectiondata.h"

using namespace QMatrixClient;

class PasswordLogin::Private
{
    public:
        Private() {reply=0;}

        QNetworkReply* reply;
        QString user;
        QString password;
        QString returned_id;
        QString returned_server;
        QString returned_token;
};

PasswordLogin::PasswordLogin(ConnectionData* connection, QString user, QString password)
    : BaseJob(connection)
    , d(new Private)
{
    d->user = user;
    d->password = password;
}

PasswordLogin::~PasswordLogin()
{
    delete d->reply;
    delete d;
}

void PasswordLogin::start()
{
    QString path = "_matrix/client/api/v1/login";
    QJsonObject json;
    json.insert("type", "m.login.password");
    json.insert("user", d->user);
    json.insert("password", d->password);
    d->reply = post(path, QJsonDocument(json) );
    connect( d->reply, &QNetworkReply::finished, this, &PasswordLogin::gotReply );
}

QString PasswordLogin::token()
{
    return d->returned_token;
}

QString PasswordLogin::id()
{
    return d->returned_id;
}

QString PasswordLogin::server()
{
    return d->returned_server;
}

void PasswordLogin::gotReply()
{
    if( d->reply->error() != QNetworkReply::NoError && d->reply->error() != QNetworkReply::ContentAccessDenied )
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
    qDebug() << data;
    QJsonObject json = data.object();
    if( !json.contains("access_token") || !json.contains("home_server") || !json.contains("user_id") )
    {
        fail( KJob::UserDefinedError+2, "Unexpected data" );
    }
    d->returned_token = json.value("access_token").toString();
    d->returned_server = json.value("home_server").toString();
    d->returned_id = json.value("user_id").toString();
    connection()->setToken(d->returned_token);
    emitResult();
}


