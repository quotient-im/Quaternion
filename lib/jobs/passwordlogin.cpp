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
        Private() {}

        QString user;
        QString password;
        QString returned_id;
        QString returned_server;
        QString returned_token;
};

PasswordLogin::PasswordLogin(ConnectionData* connection, QString user, QString password)
    : BaseJob(connection, JobHttpType::PostJob, false)
    , d(new Private)
{
    d->user = user;
    d->password = password;
}

PasswordLogin::~PasswordLogin()
{
    delete d;
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

QString PasswordLogin::apiPath()
{
    return "_matrix/client/r0/login";
}

QJsonObject PasswordLogin::data()
{
    QJsonObject json;
    json.insert("type", "m.login.password");
    json.insert("user", d->user);
    json.insert("password", d->password);
    return json;
}

void PasswordLogin::parseJson(const QJsonDocument& data)
{
    QJsonObject json = data.object();
    if( !json.contains("access_token") || !json.contains("home_server") || !json.contains("user_id") )
    {
        fail( BaseJob::UserDefinedError, "Unexpected data" );
    }
    d->returned_token = json.value("access_token").toString();
    qDebug() << d->returned_token;
    d->returned_server = json.value("home_server").toString();
    d->returned_id = json.value("user_id").toString();
    connection()->setToken(d->returned_token);
    emitResult();
}
