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

#include <QtGui/QBrush>
#include <QtGui/QColor>
#include <QtGui/QIcon>

#include "lib/connection.h"
#include "lib/room.h"
#include "../quaternionroom.h"

RoomListModel::RoomListModel(QObject* parent)
    : QAbstractListModel(parent)
{
    m_connection = 0;
}

RoomListModel::~RoomListModel()
{
}

void RoomListModel::setConnection(QMatrixClient::Connection* connection)
{
    beginResetModel();
    for( QuaternionRoom* room: m_rooms )
        room->disconnect( this );

    m_rooms.clear();

    m_connection = connection;
    connect( connection, &QMatrixClient::Connection::newRoom, this, &RoomListModel::addRoom );
    for( QMatrixClient::Room* r: connection->roomMap() )
        doAddRoom(r);

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
        this, &RoomListModel::displaynameChanged );
    connect( room, &QuaternionRoom::unreadMessagesChanged,
        this, &RoomListModel::unreadMessagesChanged );
    connect( room, &QuaternionRoom::notificationCountChanged,
        this, &RoomListModel::unreadMessagesChanged );
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
    if( role == Qt::ForegroundRole )
    {
        if( room->highlightCount() > 0 )
            return QBrush(QColor("orange"));
        if( room->hasUnreadMessages() )
            return QBrush(QColor("blue"));
        return QVariant();
    }
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
        QString result = QString("<b>%1</b><br>").arg(room->displayName());
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

void RoomListModel::displaynameChanged(QMatrixClient::Room* room)
{
    int row = m_rooms.indexOf(static_cast<QuaternionRoom*>(room));
    emit dataChanged(index(row), index(row));
}

void RoomListModel::unreadMessagesChanged(QMatrixClient::Room* room)
{
    int row = m_rooms.indexOf(static_cast<QuaternionRoom*>(room));
    emit dataChanged(index(row), index(row));
}
