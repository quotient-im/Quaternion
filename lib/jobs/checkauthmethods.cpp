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

#include "checkauthmethods.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>

#include "../connectiondata.h"

using namespace QMatrixClient;

class CheckAuthMethods::Private
{
    public:
        Private() {reply=0;}
        
        QNetworkReply* reply;
        QString session;
};

CheckAuthMethods::CheckAuthMethods(ConnectionData* connection)
    : BaseJob(connection)
    , d(new Private)
{
}

CheckAuthMethods::~CheckAuthMethods()
{
    delete d->reply;
    delete d;
}

void CheckAuthMethods::start()
{
    QString path = "_matrix/client/api/v1/login";
    d->reply = get(path);
    connect( d->reply, &QNetworkReply::finished, this, &CheckAuthMethods::gotReply );
}

QString CheckAuthMethods::session()
{
    return d->session;
}

void CheckAuthMethods::gotReply()
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
//     if( !data.object().contains("session") )
//     {
//         qDebug() << data;
//         fail( KJob::UserDefinedError+2, "Session attribute missing" );
//         return;
//     }
//     d->session = data.object().value("session").toString();
}