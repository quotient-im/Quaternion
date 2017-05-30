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

#pragma once

#include <QtCore/QDateTime>

namespace QMatrixClient
{
    class RoomEvent;
    class Room;
}

class Message
{
    public:
        Message() = default;
        Message(QMatrixClient::RoomEvent* event, QMatrixClient::Room* room);

        QMatrixClient::RoomEvent* messageEvent() const;
        bool highlight() const;
        bool isStatusMessage() const;

    private:
        QMatrixClient::RoomEvent* m_event = nullptr;
        bool m_isHighlight = false;
        bool m_isStatusMessage = true;
};
Q_DECLARE_TYPEINFO(Message, Q_MOVABLE_TYPE);
