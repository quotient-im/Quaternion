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
    class Event;
    class ConnectionPrivate;
    class ConnectionData;

    class SyncJob;
    class RoomMessagesJob;
    class PostReceiptJob;
    class MediaThumbnailJob;

    class Connection: public QObject {
            Q_OBJECT
        public:
            Connection(QUrl server, QObject* parent=0);
            virtual ~Connection();

            QHash<QString, Room*> roomMap() const;
            virtual bool isConnected();

            virtual void connectToServer( QString user, QString password );
            virtual void reconnect();
            virtual SyncJob* sync(int timeout=-1);
            virtual void postMessage( Room* room, QString type, QString message );
            virtual PostReceiptJob* postReceipt( Room* room, Event* event );
            virtual void joinRoom( QString roomAlias );
            virtual void leaveRoom( Room* room );
            virtual void getMembers( Room* room );
            virtual RoomMessagesJob* getMessages( Room* room, QString from );
            virtual MediaThumbnailJob* getThumbnail( QUrl url, int requestedWidth, int requestedHeight );

            virtual User* user(QString userId);
            virtual User* user();

        signals:
            void connected();
            void reconnected();
            void syncDone();
            void newRoom(Room* room);
            void joinedRoom(Room* room);

            void loginError(QString error);
            void connectionError(QString error);
            //void jobError(BaseJob* job);
            
        protected:
            /**
             * Access the underlying ConnectionData class
             */
            ConnectionData* connectionData();
            
            /**
             * makes it possible for derived classes to have its own User class
             */
            virtual User* createUser(QString userId);
            
            /**
             * makes it possible for derived classes to have its own Room class
             */
            virtual Room* createRoom(QString roomId);

        private:
            friend class ConnectionPrivate;
            ConnectionPrivate* d;
    };
}

#endif // QMATRIXCLIENT_CONNECTION_H