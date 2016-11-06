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

#include "lib/events/event.h"
#include "lib/events/roommessageevent.h"
#include "lib/user.h"
#include "lib/connection.h"

#include <QtCore/QDebug>

QuaternionRoom::QuaternionRoom(QMatrixClient::Connection* connection, QString roomId)
    : QMatrixClient::Room(connection, roomId)
{
    m_shown = false;
    m_cachedInput = "";
    connect( this, &QuaternionRoom::notificationCountChanged, this, &QuaternionRoom::countChanged );
    connect( this, &QuaternionRoom::highlightCountChanged, this, &QuaternionRoom::countChanged );
}

QuaternionRoom::~QuaternionRoom()
{ }

void QuaternionRoom::setShown(bool shown)
{
    if( shown == m_shown )
        return;
    m_shown = shown;
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

void QuaternionRoom::countChanged()
{
    if( m_shown )
    {
        resetNotificationCount();
        resetHighlightCount();
    }
}

const QString& QuaternionRoom::cachedInput() const
{
    return m_cachedInput;
}

void QuaternionRoom::setCachedInput(const QString& input)
{
    m_cachedInput = input;
}

bool QuaternionRoom::isHighlight(const QMatrixClient::Event* event)
{
    if( event->type() == QMatrixClient::EventType::RoomMessage )
    {
        const QMatrixClient::RoomMessageEvent* messageEvent = static_cast<const QMatrixClient::RoomMessageEvent*>(event);
        QMatrixClient::User* localUser = connection()->user();
        // Only highlight messages from other users
        if (messageEvent->senderId() != localUser->id())
        {
            if( messageEvent->body().contains(localUser->id()) )
                return true;
            QString ownDisplayname = roomMembername(localUser);
            if (messageEvent->body().contains(ownDisplayname))
                return true;
        }
    }
    return false;
}
