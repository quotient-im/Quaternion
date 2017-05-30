/**************************************************************************
 *                                                                        *
 * Copyright (C) 2016 Felix Rohrbach <kde@fxrh.de>                        *
 *                                                                        *
 * This program is free software; you can redistribute it and/or          *
 * modify it under the terms of the GNU General Public License            *
 * as published by the Free Software Foundation; either version 3         *
 * of the License, or (at your option) any later version.                 *
 *                                                                        *
 * This program is distributed in the hope that it will be useful,        *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 * GNU General Public License for more details.                           *
 *                                                                        *
 * You should have received a copy of the GNU General Public License      *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 *                                                                        *
 **************************************************************************/

#pragma once

#include <QtCore/QAbstractListModel>

namespace QMatrixClient
{
    class Connection;
    class Room;
}

class QuaternionRoom;

class RoomListModel: public QAbstractListModel
{
        Q_OBJECT
        using Connection = QMatrixClient::Connection;
    public:
        enum Roles {
            HasUnreadRole = Qt::UserRole + 1,
            HighlightCountRole,
        };

        explicit RoomListModel(QObject* parent = nullptr);

        void addConnection(Connection* connection);
        void deleteConnection(Connection* connection);
        QuaternionRoom* roomAt(int row);

        QVariant data(const QModelIndex& index, int role) const override;
        int rowCount(const QModelIndex& parent) const override;

    private slots:
        void displaynameChanged(QuaternionRoom* room);
        void unreadMessagesChanged(QuaternionRoom* room);
        void addRoom(QMatrixClient::Room* room);

    private:
        QList<Connection*> m_connections;
        QList<QuaternionRoom*> m_rooms;

        void doAddRoom(QMatrixClient::Room* r);
};
