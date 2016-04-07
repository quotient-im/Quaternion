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

#include "quaternionroom.h"

#include "message.h"
#include "lib/events/event.h"
#include "lib/connection.h"

QuaternionRoom::QuaternionRoom(QMatrixClient::Connection* connection, QString roomId)
    : QMatrixClient::Room(connection, roomId)
{
    m_shown = false;
    m_unreadMessages = true;
    connect( this, &QuaternionRoom::notificationCountChanged, this, &QuaternionRoom::countChanged );
    connect( this, &QuaternionRoom::highlightCountChanged, this, &QuaternionRoom::countChanged );
}

void QuaternionRoom::setShown(bool shown)
{
    if( shown == m_shown )
        return;
    m_shown = shown;
    if( m_shown && m_unreadMessages )
    {
        if( !messageEvents().empty() )
            markMessageAsRead( messageEvents().last() );
        m_unreadMessages = false;
        emit unreadMessagesChanged(this);
        qDebug() << displayName() << "no unread messages";
    }
    if( m_shown )
    {
        resetHighlightCount();
        resetNotificationCount();
    }
}

bool QuaternionRoom::isShown()
{
    return m_shown;
}

QList<Message*> QuaternionRoom::messages() const
{
    return m_messages;
}

bool QuaternionRoom::hasUnreadMessages()
{
    return m_unreadMessages;
}

void QuaternionRoom::processMessageEvent(QMatrixClient::Event* event)
{
    bool isNewest = messageEvents().empty() || event->timestamp() > messageEvents().last()->timestamp();
    QMatrixClient::Room::processMessageEvent(event);

    Message* message = new Message(connection(), event);
    for( int i=0; i<m_messages.count(); i++ )
    {
        if( message->timestamp() < m_messages.at(i)->timestamp() )
        {
            m_messages.insert(i, message);
            return;
        }
    }
    m_messages.append(message);
    emit newMessage(message);

    if( !isNewest )
        return;
    if( m_shown )
    {
        markMessageAsRead(event);
    }
    else if( !m_unreadMessages )
    {
        m_unreadMessages = true;
        emit unreadMessagesChanged(this);
        qDebug() << displayName() << "unread messages";
    }
}

void QuaternionRoom::processEphemeralEvent(QMatrixClient::Event* event)
{
    QMatrixClient::Room::processEphemeralEvent(event);
    QString lastReadId = lastReadEvent(connection()->user());
    if( m_unreadMessages && lastReadId == messageEvents().last()->id() )
    {
        m_unreadMessages = false;
        emit unreadMessagesChanged(this);
        qDebug() << displayName() << "no unread messages";
    }
}

void QuaternionRoom::countChanged()
{
    if( m_shown )
    {
        resetNotificationCount();
        resetHighlightCount();
    }
}

