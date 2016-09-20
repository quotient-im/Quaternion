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

#include "quaternionroom.h"

#include "message.h"
#include "lib/events/event.h"
#include "lib/connection.h"

#include <QtCore/QDebug>

QuaternionRoom::QuaternionRoom(QMatrixClient::Connection* connection, QString roomId)
    : QMatrixClient::Room(connection, roomId)
{
    m_shown = false;
    m_unreadMessages = false;
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

const QuaternionRoom::Timeline& QuaternionRoom::messages() const
{
    return m_messages;
}

bool QuaternionRoom::hasUnreadMessages()
{
    return m_unreadMessages;
}

inline Message* QuaternionRoom::makeMessage(QMatrixClient::Event* e)
{
    return new Message(connection(), e, this);
}

void QuaternionRoom::doAddNewMessageEvents(const QMatrixClient::Events& events)
{
    Room::doAddNewMessageEvents(events);

    m_messages.reserve(m_messages.size() + events.size());
    for (auto e: events)
        m_messages.push_back(makeMessage(e));

    if( m_shown )
    {
        markMessageAsRead(messageEvents().back());
    }
    else if( !m_unreadMessages )
    {
        m_unreadMessages = true;
        emit unreadMessagesChanged(this);
        qDebug() << "Room" << displayName() << ": unread messages";
    }
}

void QuaternionRoom::doAddHistoricalMessageEvents(const QMatrixClient::Events& events)
{
    Room::doAddHistoricalMessageEvents(events);

    m_messages.reserve(m_messages.size() + events.size());
    for (auto e: events)
        m_messages.push_front(makeMessage(e));
}

void QuaternionRoom::processEphemeralEvent(QMatrixClient::Event* event)
{
    QMatrixClient::Room::processEphemeralEvent(event);
    QString lastReadId = lastReadEvent(connection()->user());
    if( m_unreadMessages &&
            (messageEvents().isEmpty() || lastReadId == messageEvents().last()->id()) )
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

