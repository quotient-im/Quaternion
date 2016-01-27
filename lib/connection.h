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

#ifndef QMATRIXCLIENT_CONNECTION_H
#define QMATRIXCLIENT_CONNECTION_H

#include <QtCore/QObject>

namespace QMatrixClient
{
    class Room;
    class User;
    class ConnectionPrivate;

    class SyncJob;

    class Connection: public QObject {
            Q_OBJECT
        public:
            Connection(QUrl server, QObject* parent=0);
            virtual ~Connection();

            QHash<QString, Room*> roomMap() const;
            bool isConnected();

            void connectToServer( QString user, QString password );
            void reconnect();
            SyncJob* sync();
            void postMessage( Room* room, QString type, QString message );
            void joinRoom( QString roomAlias );
            void leaveRoom( Room* room );
            void getMembers( Room* room );

            User* user(QString userId);

        signals:
            void connected();
            void reconnected();
            void syncDone();
            void newRoom(Room* room);
            void joinedRoom(Room* room);

            void loginError(QString error);
            void connectionError(QString error);
            //void jobError(BaseJob* job);

        private:
            ConnectionPrivate* d;
    };
}

#endif // QMATRIXCLIENT_CONNECTION_H