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

#include "message.h"

#include "lib/events/roommessageevent.h"
#include "lib/user.h"
#include "lib/connection.h"
#include "lib/room.h"

Message::Message(QMatrixClient::RoomEvent* event, QMatrixClient::Room* room)
    : m_event(event)
    , m_isHighlight(false)
    , m_isStatusMessage(true)
{
    using namespace QMatrixClient;
    if( event->type() == EventType::RoomMessage )
    {
        m_isStatusMessage = false;
        Q_ASSERT(room);

        auto* msgEvent = static_cast<RoomMessageEvent*>(event);
        User* localUser = room->connection()->user();
        // Only highlight messages from other users
        m_isHighlight = msgEvent->senderId() != localUser->id() &&
            (msgEvent->plainBody().contains(localUser->id()) ||
             msgEvent->plainBody().contains(room->roomMembername(localUser))
            );
    }
}

QMatrixClient::RoomEvent* Message::messageEvent() const
{
    return m_event;
}

bool Message::highlight() const
{
    return m_isHighlight;
}

bool Message::isStatusMessage() const
{
    return m_isStatusMessage;
}
