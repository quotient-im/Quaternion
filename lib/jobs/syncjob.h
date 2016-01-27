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

#ifndef QMATRIXCLIENT_SYNCJOB_H
#define QMATRIXCLIENT_SYNCJOB_H

#include "basejob.h"

namespace QMatrixClient
{
    class ConnectionData;
    class Room;
    class Event;
    class SyncJob: public BaseJob
    {
            Q_OBJECT
        public:
            SyncJob(ConnectionData* connection, QString since=QString());
            virtual ~SyncJob();
            
            void setFilter(QString filter);
            void setFullState(bool full);
            void setPresence(QString presence);
            void setTimeout(int timeout);

            QHash<QString, QJsonObject> joinedRooms();
            QHash<QString, QJsonObject> invitedRooms();
            QHash<QString, QJsonObject> leftRooms();

        protected:
            QString apiPath();
            QUrlQuery query();
            void parseJson(const QJsonDocument& data);

        private:
            class Private;
            Private* d;
    };
}

#endif // QMATRIXCLIENT_GETEVENTSJOB_H