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

#include "../quaternionroom.h"
#include "lib/connection.h"
#include "lib/util.h"

#include <QtCore/QAbstractListModel>

class RoomListModel: public QAbstractListModel
{
        Q_OBJECT
        template <typename T>
        using ConnectionsGuard = QMatrixClient::ConnectionsGuard<T>;
    public:
        enum Roles {
            HasUnreadRole = Qt::UserRole + 1,
            HighlightCountRole, JoinStateRole
        };

        explicit RoomListModel(QObject* parent = nullptr);

        void addConnection(QMatrixClient::Connection* connection);
        void deleteConnection(QMatrixClient::Connection* connection);

        QuaternionRoom* roomAt(QModelIndex index) const;
        QModelIndex indexOf(QuaternionRoom* room) const;

        QVariant data(const QModelIndex& index, int role) const override;
        int rowCount(const QModelIndex& parent) const override;

    private slots:
        void displaynameChanged(QuaternionRoom* room);
        void unreadMessagesChanged(QuaternionRoom* room);
        void refresh(QuaternionRoom* room, const QVector<int>& roles = {});

        void updateRoom(QMatrixClient::Room* room,
                        QMatrixClient::Room* prev);
        void deleteRoom(QMatrixClient::Room* room);

    private:
        std::vector<ConnectionsGuard<QMatrixClient::Connection>> m_connections;
        std::vector<ConnectionsGuard<QuaternionRoom>> m_rooms;

        void doAddRoom(QMatrixClient::Room* r);
        void connectRoomSignals(QuaternionRoom* room);
};
