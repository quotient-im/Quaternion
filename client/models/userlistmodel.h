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

namespace Quotient
{
    class Connection;
    class Room;
    class User;
}

class UserListModel: public QAbstractListModel
{
        Q_OBJECT
    public:
        using User = Quotient::User;

        UserListModel(QObject* parent = nullptr);
        virtual ~UserListModel();

        void setRoom(Quotient::Room* room);
        User* userAt(QModelIndex index);

        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        int rowCount(const QModelIndex& parent=QModelIndex()) const override;
    
    signals:
        void membersChanged(); //< Reflection of Room::memberListChanged

    public slots:
        void filter(const QString& filterString);

    private slots:
        void userAdded(User* user);
        void userRemoved(User* user);
        void refresh(User* user, QVector<int> roles = {});
        void avatarChanged(User* user, const Quotient::Room* context);

    private:
        Quotient::Room* m_currentRoom;
        QList<User*> m_users;

        int findUserPos(User* user) const;
        int findUserPos(const QString& username) const;
};
