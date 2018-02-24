/**************************************************************************
 *                                                                        *
 * Copyright (C) 2015 Felix Rohrbach <kde@fxrh.de>                        *
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
    class User;
}

class UserListModel: public QAbstractListModel
{
        Q_OBJECT
    public:
        using User = QMatrixClient::User;

        UserListModel(QObject* parent = nullptr);
        virtual ~UserListModel();

    void setRoom(QMatrixClient::Room* room);

        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        int rowCount(const QModelIndex& parent=QModelIndex()) const override;

    private slots:
        void userAdded(User* user);
        void userRemoved(User* user);
        void refresh(User* user, QVector<int> roles = {});
        void avatarChanged(User* user, const QMatrixClient::Room* context);

    private:
        QMatrixClient::Room* m_currentRoom;
        QList<User*> m_users;

        int findUserPos(User* user) const;
        int findUserPos(const QString& username) const;
};
