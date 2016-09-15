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

#include "messageeventmodel.h"

#include <QtCore/QStringBuilder>
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
#include "lib/events/roomtopicevent.h"
#include "lib/events/unknownevent.h"

MessageEventModel::MessageEventModel(QObject* parent)
    : QAbstractListModel(parent)
{
    m_currentRoom = nullptr;
    m_connection = nullptr;
}

MessageEventModel::~MessageEventModel()
{
}

void MessageEventModel::changeRoom(QuaternionRoom* room)
{
    beginResetModel();
    if( m_currentRoom )
        m_currentRoom->disconnect( this );

    m_currentRoom = room;
    if( room )
    {
        m_currentMessages = m_currentRoom->messages();
        connect( m_currentRoom, &QuaternionRoom::newMessage, this, &MessageEventModel::newMessage );
        qDebug() << "connected" << room;
    }
    else
    {
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
    using namespace QMatrixClient;
    if( index.row() < 0 || index.row() >= m_currentMessages.count() || !m_connection )
        return QVariant();

    Message* message = m_currentMessages.at(index.row());;
    Event* event = message->messageEvent();

    if( role == Qt::DisplayRole )
    {
        if( event->type() == EventType::RoomMessage )
        {
            RoomMessageEvent* e = static_cast<RoomMessageEvent*>(event);
            User* user = m_connection->user(e->userId());
            return QString("%1 (%2): %3").arg(user->name()).arg(user->id()).arg(e->body());
        }
        if( event->type() == EventType::RoomMember )
        {
            RoomMemberEvent* e = static_cast<RoomMemberEvent*>(event);
            switch( e->membership() )
            {
                case MembershipType::Join:
                    return QString("%1 (%2) joined the room").arg(e->displayName(), e->userId());
                case MembershipType::Leave:
                    return QString("%1 (%2) left the room").arg(e->displayName(), e->userId());
                case MembershipType::Ban:
                    return QString("%1 (%2) was banned from the room").arg(e->displayName(), e->userId());
                case MembershipType::Invite:
                    return QString("%1 (%2) was invited to the room").arg(e->displayName(), e->userId());
                case MembershipType::Knock:
                    return QString("%1 (%2) knocked").arg(e->displayName(), e->userId());
            }
        }
        if( event->type() == EventType::RoomAliases )
        {
            RoomAliasesEvent* e = static_cast<RoomAliasesEvent*>(event);
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
        if( event->type() == EventType::RoomMessage )
        {
            auto msgType = static_cast<RoomMessageEvent*>(event)->msgtype();
            if( msgType == MessageEventType::Image )
                return "image";
            else if( msgType == MessageEventType::Emote )
                return "emote";
            return "message";
        }
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
        if( event->type() == EventType::RoomMessage )
        {
            RoomMessageEvent* e = static_cast<RoomMessageEvent*>(event);
            return m_currentRoom->roomMembername(e->userId());
        }
        return QVariant();
    }

    if (role == ContentTypeRole  || role == ContentRole)
    {
        if( event->type() == EventType::RoomMessage )
        {
            using namespace MessageEventContent;

            RoomMessageEvent* e = static_cast<RoomMessageEvent*>(event);
            switch (e->msgtype())
            {
            case MessageEventType::Emote:
            case MessageEventType::Text:
            case MessageEventType::Notice:
                {
                    QString body;
                    QString contentType;
                    auto textContent = static_cast<TextContent*>(e->content());
                    if (textContent && textContent->mimeType.inherits("text/html"))
                    {
                        body = textContent->body;
                        contentType = "text/html";
                    }
                    else
                    {
                        body = e->plainBody();
                        contentType = "text/plain";
                    }

                    if (role == ContentRole)
                        return body;
                    else
                        return contentType;
                }
            case MessageEventType::Image:
                {
                    auto content = static_cast<ImageContent*>(e->content());
                    if (role == ContentRole)
                        return QUrl("image://mtx/" +
                                    content->url.host() + content->url.path());
                    else
                        return content->mimetype.name();
                }
            case MessageEventType::File:
            case MessageEventType::Location:
            case MessageEventType::Video:
            case MessageEventType::Audio:
                {
                    auto fileInfo = static_cast<FileInfo*>(e->content());
                    if (role == ContentRole)
                        return e->body(); // TODO
                    else
                        return fileInfo ? fileInfo->mimetype.name() : "unknown";
                }
            default:
                if (role == ContentRole)
                    return e->body();
                else
                    return "unknown";
            }
        }
        if( event->type() == EventType::RoomMember )
        {
            RoomMemberEvent* e = static_cast<RoomMemberEvent*>(event);
            switch( e->membership() )
            {
                case MembershipType::Join:
                    return QString("%1 (%2) joined the room").arg(e->displayName(), e->userId());
                case MembershipType::Leave:
                    return QString("%1 (%2) left the room").arg(e->displayName(), e->userId());
                case MembershipType::Ban:
                    return QString("%1 (%2) was banned from the room").arg(e->displayName(), e->userId());
                case MembershipType::Invite:
                    return QString("%1 (%2) was invited to the room").arg(e->displayName(), e->userId());
                case MembershipType::Knock:
                    return QString("%1 (%2) knocked").arg(e->displayName(), e->userId());
            }
        }
        if( event->type() == EventType::RoomAliases )
        {
            auto e = static_cast<RoomAliasesEvent*>(event);
            return QString("Current aliases: %1").arg(e->aliases().join(", "));
        }
        if( event->type() == EventType::RoomTopic )
        {
            auto e = static_cast<RoomTopicEvent*>(event);
            return QString("%1 has set the topic to: %2")
                    .arg(m_currentRoom->roomMembername(e->senderId()), e->topic());
        }
        return "Unknown Event";
    }

    if( role == HighlightRole )
    {
        return message->highlight();
    }
//     if( event->type() == EventType::Unknown )
//     {
//         UnknownEvent* e = static_cast<UnknownEvent*>(event);
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
    roles[ContentTypeRole] = "contentType";
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
    auto pos = std::distance(m_currentMessages.begin(),
            QMatrixClient::findInsertionPos(m_currentMessages, message));
    beginInsertRows(QModelIndex(), pos, pos);
    m_currentMessages.insert(pos, message);
    endInsertRows();
}
