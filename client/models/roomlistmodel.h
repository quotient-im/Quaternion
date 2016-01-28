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

#ifndef ROOMLISTMODEL_H
#define ROOMLISTMODEL_H

#include <QtCore/QAbstractListModel>

namespace QMatrixClient
{
    class Connection;
    class Room;
}

class RoomListModel: public QAbstractListModel
{
        Q_OBJECT
    public:
        RoomListModel(QObject* parent=0);
        virtual ~RoomListModel();

        void setConnection(QMatrixClient::Connection* connection);
        QMatrixClient::Room* roomAt(int row);

        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        int rowCount(const QModelIndex& parent=QModelIndex()) const override;

    private slots:
        void aliasChanged(QMatrixClient::Room* room);
        void addRoom(QMatrixClient::Room* room);

    private:
        QMatrixClient::Connection* m_connection;
        QList<QMatrixClient::Room*> m_rooms;
};

#endif // ROOMLISTMODEL_H