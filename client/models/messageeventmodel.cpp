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

#include "messageeventmodel.h"

#include <QtCore/QDebug>

#include "../message.h"
#include "../quaternionroom.h"
#include "lib/connection.h"
#include "lib/room.h"
#include "lib/user.h"
#include "lib/events/event.h"
#include "lib/events/roommessageevent.h"
#include "lib/events/roommemberevent.h"
#include "lib/events/roomaliasesevent.h"
#include "lib/events/unknownevent.h"

MessageEventModel::MessageEventModel(QObject* parent)
    : QAbstractListModel(parent)
{
    m_currentRoom = 0;
    m_connection = 0;
}

MessageEventModel::~MessageEventModel()
{
}

void MessageEventModel::changeRoom(QMatrixClient::Room* room)
{
    beginResetModel();
    if( m_currentRoom )
    {
        disconnect( m_currentRoom, &QuaternionRoom::newMessage, this, &MessageEventModel::newMessage );
    }
    if( room )
    {
        m_currentRoom = static_cast<QuaternionRoom*>(room);
        m_currentMessages = m_currentRoom->messages();
        connect( m_currentRoom, &QuaternionRoom::newMessage, this, &MessageEventModel::newMessage );
        qDebug() << "connected" << room;
    }
    else
    {
        m_currentRoom = 0;
        m_currentMessages.clear();
    }
    endResetModel();
}

void MessageEventModel::setConnection(QMatrixClient::Connection* connection)
{
    m_connection = connection;
}

// QModelIndex LogMessageModel::index(int row, int column, const QModelIndex& parent) const
// {
//     if( parent.isValid() )
//         return QModelIndex();
//     if( row < 0 || row >= m_currentMessages.count() )
//         return QModelIndex();
//     return createIndex(row, column, m_currentMessages.at(row));
// }
//
// LogMessageModel::parent(const QModelIndex& index) const
// {
//     return QModelIndex();
// }

int MessageEventModel::rowCount(const QModelIndex& parent) const
{
    if( parent.isValid() )
        return 0;
    return m_currentMessages.count();
}

QVariant MessageEventModel::data(const QModelIndex& index, int role) const
{
    if( index.row() < 0 || index.row() >= m_currentMessages.count() )
        return QVariant();

    Message* message = m_currentMessages.at(index.row());;
    QMatrixClient::Event* event = message->messageEvent();

    if( role == Qt::DisplayRole )
    {
        if( event->type() == QMatrixClient::EventType::RoomMessage )
        {
            QMatrixClient::RoomMessageEvent* e = static_cast<QMatrixClient::RoomMessageEvent*>(event);
            QMatrixClient::User* user = m_connection->user(e->userId());
            return QString("%1 (%2): %3").arg(user->name()).arg(user->id()).arg(e->body());
        }
        if( event->type() == QMatrixClient::EventType::RoomMember )
        {
            QMatrixClient::RoomMemberEvent* e = static_cast<QMatrixClient::RoomMemberEvent*>(event);
            switch( e->membership() )
            {
                case QMatrixClient::MembershipType::Join:
                    return QString("%1 (%2) joined the room").arg(e->displayName(), e->userId());
                case QMatrixClient::MembershipType::Leave:
                    return QString("%1 (%2) left the room").arg(e->displayName(), e->userId());
                case QMatrixClient::MembershipType::Ban:
                    return QString("%1 (%2) was banned from the room").arg(e->displayName(), e->userId());
                case QMatrixClient::MembershipType::Invite:
                    return QString("%1 (%2) was invited to the room").arg(e->displayName(), e->userId());
                case QMatrixClient::MembershipType::Knock:
                    return QString("%1 (%2) knocked").arg(e->displayName(), e->userId());
            }
        }
        if( event->type() == QMatrixClient::EventType::RoomAliases )
        {
            QMatrixClient::RoomAliasesEvent* e = static_cast<QMatrixClient::RoomAliasesEvent*>(event);
            return QString("Current aliases: %1").arg(e->aliases().join(", "));
        }
        return "Unknown Event";
    }

    if( role == Qt::ToolTipRole )
    {
        return event->originalJson();
    }

    if( role == EventTypeRole )
    {
        if( event->type() == QMatrixClient::EventType::RoomMessage )
            return "message";
        return "other";
    }

    if( role == TimeRole )
    {
        return event->timestamp();
    }

    if( role == DateRole )
    {
        return event->timestamp().toLocalTime().date();
    }

    if( role == AuthorRole )
    {
        if( event->type() == QMatrixClient::EventType::RoomMessage )
        {
            QMatrixClient::RoomMessageEvent* e = static_cast<QMatrixClient::RoomMessageEvent*>(event);
            return m_connection->user(e->userId())->displayname();
        }
        return QVariant();
    }

    if( role == ContentRole )
    {
        if( event->type() == QMatrixClient::EventType::RoomMessage )
        {
            QMatrixClient::RoomMessageEvent* e = static_cast<QMatrixClient::RoomMessageEvent*>(event);
            return e->body();
        }
        if( event->type() == QMatrixClient::EventType::RoomMember )
        {
            QMatrixClient::RoomMemberEvent* e = static_cast<QMatrixClient::RoomMemberEvent*>(event);
            switch( e->membership() )
            {
                case QMatrixClient::MembershipType::Join:
                    return QString("%1 (%2) joined the room").arg(e->displayName(), e->userId());
                case QMatrixClient::MembershipType::Leave:
                    return QString("%1 (%2) left the room").arg(e->displayName(), e->userId());
                case QMatrixClient::MembershipType::Ban:
                    return QString("%1 (%2) was banned from the room").arg(e->displayName(), e->userId());
                case QMatrixClient::MembershipType::Invite:
                    return QString("%1 (%2) was invited to the room").arg(e->displayName(), e->userId());
                case QMatrixClient::MembershipType::Knock:
                    return QString("%1 (%2) knocked").arg(e->displayName(), e->userId());
            }
        }
        if( event->type() == QMatrixClient::EventType::RoomAliases )
        {
            QMatrixClient::RoomAliasesEvent* e = static_cast<QMatrixClient::RoomAliasesEvent*>(event);
            return QString("Current aliases: %1").arg(e->aliases().join(", "));
        }
        return "Unknown Event";
    }

    if( role == HighlightRole )
    {
        return message->highlight();
    }
//     if( event->type() == QMatrixClient::EventType::Unknown )
//     {
//         QMatrixClient::UnknownEvent* e = static_cast<QMatrixClient::UnknownEvent*>(event);
//         return "Unknown Event: " + e->typeString() + "(" + e->content();
//     }
    return QVariant();
}

QHash<int, QByteArray> MessageEventModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();
    roles[EventTypeRole] = "eventType";
    roles[TimeRole] = "time";
    roles[DateRole] = "date";
    roles[AuthorRole] = "author";
    roles[ContentRole] = "content";
    roles[HighlightRole] = "highlight";
    return roles;
}

void MessageEventModel::newMessage(Message* message)
{
    //qDebug() << "Message: " << message;
    if( message->messageEvent()->type() == QMatrixClient::EventType::Typing )
    {
        return;
    }
    for( int i=0; i<m_currentMessages.count(); i++ )
    {
        if( message->timestamp() < m_currentMessages.at(i)->timestamp() )
        {
            beginInsertRows(QModelIndex(), i, i);
            m_currentMessages.insert(i, message);
            endInsertRows();
            return;
        }
    }
    beginInsertRows(QModelIndex(), m_currentMessages.count(), m_currentMessages.count());
    m_currentMessages.append(message);
    endInsertRows();
}
