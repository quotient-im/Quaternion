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

#include "roomlistmodel.h"

#include <QtGui/QIcon>
#include <QtCore/QDebug>

#include "../quaternionconnection.h"
#include "../quaternionroom.h"

RoomListModel::RoomListModel(QObject* parent)
    : QAbstractListModel(parent)
{ }

void RoomListModel::addConnection(QuaternionConnection* connection)
{
    Q_ASSERT(connection);

    beginResetModel();
    m_connections.push_back(connection);
    connect( connection, &QuaternionConnection::newRoom,
             this, &RoomListModel::addRoom );
    connect( connection, &QuaternionConnection::loggedOut,
             this, [=]{ deleteConnection(connection); } );
    for( auto r: connection->roomMap() )
        doAddRoom(r);
    endResetModel();
}

void RoomListModel::deleteConnection(QuaternionConnection* connection)
{
    Q_ASSERT(connection);

    beginResetModel();
    connection->disconnect(this);
    for( QuaternionRoom* room: m_rooms )
        room->disconnect( this );
    m_rooms.erase(
        std::remove_if(m_rooms.begin(), m_rooms.end(),
            [=](const QuaternionRoom* r) { return r->connection() == connection; }),
        m_rooms.end());
    endResetModel();
}

QuaternionRoom* RoomListModel::roomAt(int row)
{
    return m_rooms.at(row);
}

void RoomListModel::addRoom(QMatrixClient::Room* room)
{
    beginInsertRows(QModelIndex(), m_rooms.count(), m_rooms.count());
    doAddRoom(room);
    endInsertRows();
}

void RoomListModel::doAddRoom(QMatrixClient::Room* r)
{
    QuaternionRoom* room = static_cast<QuaternionRoom*>(r);
    m_rooms.append(room);
    connect( room, &QuaternionRoom::displaynameChanged,
             this, [=]{ displaynameChanged(room); } );
    connect( room, &QuaternionRoom::unreadMessagesChanged,
             this, [=]{ unreadMessagesChanged(room); } );
    connect( room, &QuaternionRoom::notificationCountChanged,
             this, [=]{ unreadMessagesChanged(room); } );
}

int RoomListModel::rowCount(const QModelIndex& parent) const
{
    if( parent.isValid() )
        return 0;
    return m_rooms.count();
}

QVariant RoomListModel::data(const QModelIndex& index, int role) const
{
    if( !index.isValid() )
        return QVariant();

    if( index.row() >= m_rooms.count() )
    {
        qDebug() << "UserListModel: something wrong here...";
        return QVariant();
    }
    QuaternionRoom* room = m_rooms.at(index.row());
    if( role == Qt::DisplayRole )
    {
        return room->displayName();
    }
    if( role == HasUnreadRole )
        return room->hasUnreadMessages();
    if( role == HighlightCountRole )
        return room->highlightCount();
    if( role == Qt::DecorationRole )
    {
        switch( room->joinState() )
        {
            case QMatrixClient::JoinState::Join:
                return QIcon(":/irc-channel-joined.svg");
            case QMatrixClient::JoinState::Invite:
                return QIcon(":/irc-channel-invited.svg");
            case QMatrixClient::JoinState::Leave:
                return QIcon(":/irc-channel-parted.svg");
        }
    }
    if( role == Qt::ToolTipRole )
    {
        QString result = QString("<b>%1</b><br>").arg(room->canonicalAlias());
        result += tr("Room users: %1<br>").arg(room->users().count());
        if (room->highlightCount() > 0)
            result += tr("Unread mentions: %1<br>").arg(room->highlightCount());
        result += tr("Room ID: %1<br>").arg(room->id());
        if( room->joinState() == QMatrixClient::JoinState::Join )
            result += tr("You joined this room");
        else if( room->joinState() == QMatrixClient::JoinState::Leave )
            result += tr("You left this room");
        else
            result += tr("You were invited into this room");
        return result;
    }
    return QVariant();
}

void RoomListModel::displaynameChanged(QuaternionRoom* room)
{
    int row = m_rooms.indexOf(room);
    emit dataChanged(index(row), index(row));
}

void RoomListModel::unreadMessagesChanged(QuaternionRoom* room)
{
    int row = m_rooms.indexOf(room);
    emit dataChanged(index(row), index(row));
}

