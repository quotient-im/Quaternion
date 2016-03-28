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

#ifndef USERLISTMODEL_H
#define USERLISTMODEL_H

#include <QtCore/QAbstractListModel>

namespace QMatrixClient
{
    class Connection;
    class Room;
    class User;
}

class UserListModel: public QAbstractListModel
{
        Q_OBJECT
    public:
        UserListModel(QObject* parent=0);
        virtual ~UserListModel();

        void setConnection(QMatrixClient::Connection* connection);
        void setRoom(QMatrixClient::Room* room);

        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        int rowCount(const QModelIndex& parent=QModelIndex()) const override;

    private slots:
        void userAdded(QMatrixClient::User* user);
        void userRemoved(QMatrixClient::User* user);
        void userRenamed(QMatrixClient::User* user, QString);
        void avatarChanged(QMatrixClient::User* user);

    private:
        QMatrixClient::Connection* m_connection;
        QMatrixClient::Room* m_currentRoom;
        QList<QMatrixClient::User*> m_users;
};

#endif // USERLISTMODEL_H
