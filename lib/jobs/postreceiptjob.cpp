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

#include "postreceiptjob.h"
#include "../room.h"
#include "../connectiondata.h"

#include <QtNetwork/QNetworkReply>

using namespace QMatrixClient;

class PostReceiptJob::Private
{
    public:
        Private() {}

        QString roomId;
        QString eventId;
};

PostReceiptJob::PostReceiptJob(ConnectionData* connection, QString roomId, QString eventId)
    : BaseJob(connection, JobHttpType::PostJob)
    , d(new Private)
{
    d->roomId = roomId;
    d->eventId = eventId;
}

PostReceiptJob::~PostReceiptJob()
{
    delete d;
}

QString PostReceiptJob::apiPath()
{
    return QString("/_matrix/client/r0/rooms/%1/receipt/m.read/%2").arg(d->roomId).arg(d->eventId);
}
