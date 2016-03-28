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

#ifndef QMATRIXCLIENT_CONNECTIONPRIVATE_H
#define QMATRIXCLIENT_CONNECTIONPRIVATE_H

class KJob;

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QJsonObject>

#include "connection.h"
#include "connectiondata.h"
#include "jobs/syncjob.h"

namespace QMatrixClient
{
    class Connection;
    class Event;
    class State;
    class User;

    class ConnectionPrivate : public QObject
    {
            Q_OBJECT
        public:
            ConnectionPrivate(Connection* parent);
            ~ConnectionPrivate();

            void processState( State* state );
            void processRooms( const QList<SyncRoomData>& data );

            Connection* q;
            ConnectionData* data;
            QHash<QString, Room*> roomMap;
            QHash<QString, User*> userMap;
            bool isConnected;
            QString username;
            QString password;
            QString userId;

        public slots:
            void connectDone(KJob* job);
            void reconnectDone(KJob* job);
            void syncDone(KJob* job);
            void gotJoinRoom(KJob* job);
            void gotRoomMembers(KJob* job);
    };
}

#endif // QMATRIXCLIENT_CONNECTIONPRIVATE_H