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

#include "userlistmodel.h"

#include <QtCore/QDebug>
#include <QtCore/QVector>
#include <QtGui/QPixmap>

#include "lib/connection.h"
#include "lib/room.h"
#include "lib/user.h"

class MemberNameSorter
{
        using Room = QMatrixClient::Room;
        using User = QMatrixClient::User;
    public:
        MemberNameSorter(Room* r) : room(r) { }

        bool operator()(User* u1, User* u2) const
        {
            auto n1 = room->roomMembername(u1);
            auto n2 = room->roomMembername(u2);
            if (n1[0] == '@')
                n1.remove(0, 1);
            if (n2[0] == '@')
                n2.remove(0, 1);
            return n1 < n2;
        }

        template <typename ContT>
        typename ContT::size_type lowerBoundIndex(const ContT& c,
                                                  typename ContT::value_type v) const
        {
            return  std::lower_bound(c.begin(), c.end(), v, *this) - c.begin();
        }

    private:
        Room* room;
};


UserListModel::UserListModel(QObject* parent)
    : QAbstractListModel(parent)
{
    m_currentRoom = nullptr;
}

UserListModel::~UserListModel()
{
}

void UserListModel::setRoom(QMatrixClient::Room* room)
{
    if (m_currentRoom == room)
        return;

    using namespace QMatrixClient;
    beginResetModel();
    if( m_currentRoom )
    {
        m_currentRoom->disconnect( this );
        for( User* user: m_users )
            user->disconnect( this );
        m_users.clear();
    }
    m_currentRoom = room;
    if( m_currentRoom )
    {
        connect( m_currentRoom, &Room::userAdded, this, &UserListModel::userAdded );
        connect( m_currentRoom, &Room::userRemoved, this, &UserListModel::userRemoved );
        connect( m_currentRoom, &Room::memberRenamed, this, &UserListModel::memberRenamed );
        m_users = m_currentRoom->users();
        std::sort(m_users.begin(), m_users.end(), MemberNameSorter(room));
        for( User* user: m_users )
        {
            connect( user, &User::avatarChanged, this, &UserListModel::avatarChanged );
        }
        qDebug() << m_users.count() << "user(s) in the room";
    }
    endResetModel();
}

QVariant UserListModel::data(const QModelIndex& index, int role) const
{
    if( !index.isValid() )
        return QVariant();

    if( index.row() >= m_users.count() )
    {
        qDebug() << "UserListModel, something's wrong: index.row() >= m_users.count()";
        return QVariant();
    }
    QMatrixClient::User* user = m_users.at(index.row());
    if( role == Qt::DisplayRole )
    {
        return m_currentRoom->roomMembername(user);
    }
    if( role == Qt::DecorationRole )
    {
        return user->avatar(25,25);
    }
    return QVariant();
}

int UserListModel::rowCount(const QModelIndex& parent) const
{
    if( parent.isValid() )
        return 0;

    return m_users.count();
}

void UserListModel::userAdded(QMatrixClient::User* user)
{
    auto pos = MemberNameSorter(m_currentRoom).lowerBoundIndex(m_users, user);
    beginInsertRows(QModelIndex(), pos, pos);
    m_users.insert(pos, user);
    endInsertRows();
    connect( user, &QMatrixClient::User::avatarChanged, this, &UserListModel::avatarChanged );
}

void UserListModel::userRemoved(QMatrixClient::User* user)
{
    auto pos = MemberNameSorter(m_currentRoom).lowerBoundIndex(m_users, user);
    if (pos != m_users.size())
    {
        beginRemoveRows(QModelIndex(), pos, pos);
        m_users.removeAt(pos);
        endRemoveRows();
        disconnect( user, &QMatrixClient::User::avatarChanged, this, &UserListModel::avatarChanged );
    } else
        qWarning() << "Trying to remove a room member not in the user list";
}

void UserListModel::memberRenamed(QMatrixClient::User *user)
{
    auto pos = MemberNameSorter(m_currentRoom).lowerBoundIndex(m_users, user);
    if ( pos != m_users.size() )
        emit dataChanged(index(pos), index(pos), {Qt::DisplayRole} );
    else
        qWarning() << "Trying to access a room member not in the user list";
}

void UserListModel::avatarChanged(QMatrixClient::User* user)
{
    auto pos = MemberNameSorter(m_currentRoom).lowerBoundIndex(m_users, user);
    if ( pos != m_users.size() )
        emit dataChanged(index(pos), index(pos), {Qt::DecorationRole} );
    else
        qWarning() << "Trying to access a room member not in the user list";
}

