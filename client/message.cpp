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

#include "message.h"

#include "lib/events/event.h"
#include "lib/events/roommessageevent.h"
#include "lib/user.h"
#include "lib/connection.h"
#include "lib/room.h"

Message::Message(QMatrixClient::Connection* connection,
                 QMatrixClient::Event* event,
                 QMatrixClient::Room* room)
    : m_connection(connection)
    , m_event(event)
    , m_isHighlight(false)
    , m_isStatusMessage(true)
{
    using namespace QMatrixClient;
    if( event->type() == EventType::RoomMessage )
    {
        m_isStatusMessage = false;
        RoomMessageEvent* messageEvent = static_cast<RoomMessageEvent*>(event);
        User* user = m_connection->user();
        if( messageEvent->body().contains(user->id()) )
        {
            m_isHighlight = true;
        }
        if (room)
        {
            QString ownDisplayname = room->roomMembername(user);
            if (messageEvent->body().contains(ownDisplayname))
                m_isHighlight = true;
        }
    }
}

Message::~Message()
{
}

QMatrixClient::Event* Message::event()
{
    return m_event;
}

QDateTime Message::timestamp()
{
    return m_event->timestamp();
}

bool Message::highlight()
{
    return m_isHighlight;
}

bool Message::isStatusMessage()
{
    return m_isStatusMessage;
}