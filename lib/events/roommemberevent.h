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

#ifndef QMATRIXCLIENT_ROOMMEMBEREVENT_H
#define QMATRIXCLIENT_ROOMMEMBEREVENT_H

#include <QtCore/QJsonObject>
#include <QtCore/QUrl>

#include "event.h"

namespace QMatrixClient
{
    enum class MembershipType {Invite, Join, Knock, Leave, Ban};

    class RoomMemberEvent: public Event
    {
        public:
            RoomMemberEvent();
            virtual ~RoomMemberEvent();

            MembershipType membership() const;
            QString userId() const;
            QString displayName() const;
            QUrl avatarUrl() const;

            static RoomMemberEvent* fromJson(const QJsonObject& obj);

        private:
            class Private;
            Private* d;
    };
}

#endif // QMATRIXCLIENT_ROOMMEMBEREVENT_H
