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

#ifndef QMATRIXCLIENT_ROOMMESSAGESJOB_H
#define QMATRIXCLIENT_ROOMMESSAGESJOB_H

#include "basejob.h"

namespace QMatrixClient
{
    class Room;
    class Event;

    enum class FetchDirectory { Backwards, Forward };

    class RoomMessagesJob: public BaseJob
    {
            Q_OBJECT
        public:
            RoomMessagesJob(ConnectionData* data, Room* room, QString from, FetchDirectory dir = FetchDirectory::Backwards, int limit=10);
            virtual ~RoomMessagesJob();

            QList<Event*> events();
            QString end();

        protected:
            QString apiPath();
            QUrlQuery query();
            void parseJson(const QJsonDocument& data);

        private:
            class Private;
            Private* d;
    };
}

#endif // QMATRIXCLIENT_ROOMMESSAGESJOB_H