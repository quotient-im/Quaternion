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

#ifndef ROOMLISTMODEL_H
#define ROOMLISTMODEL_H

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
    public:
        enum Roles {
            HasUnreadRole = Qt::UserRole + 1,
            HighlightCountRole,
        };

        RoomListModel(QObject* parent = nullptr);
        virtual ~RoomListModel();

        void setConnection(QMatrixClient::Connection* connection);
        QuaternionRoom* roomAt(int row);

        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        int rowCount(const QModelIndex& parent=QModelIndex()) const override;

    private slots:
        void displaynameChanged(QMatrixClient::Room* room);
        void unreadMessagesChanged(QMatrixClient::Room* room);
        void addRoom(QMatrixClient::Room* room);

    private:
        QMatrixClient::Connection* m_connection;
        QList<QuaternionRoom*> m_rooms;

        void doAddRoom(QMatrixClient::Room* r);
};

#endif // ROOMLISTMODEL_H
