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

#ifndef QUATERNIONROOM_H
#define QUATERNIONROOM_H

#include "lib/room.h"

class QuaternionRoom: public QMatrixClient::Room
{
        Q_OBJECT
    public:
        QuaternionRoom(QMatrixClient::Connection* connection, QString roomId);

        /**
         * set/get whether this room is currently show to the user.
         * This is used to mark messages as read.
         */
        void setShown(bool shown);
        bool isShown();

        bool hasUnreadMessages();

    signals:
        void unreadMessagesChanged(QuaternionRoom* room);

    protected:
        virtual void processMessageEvent(QMatrixClient::Event* event) override;
        virtual void processEphemeralEvent(QMatrixClient::Event* event) override;

    private slots:
        void countChanged();

    private:
        bool m_shown;
        bool m_unreadMessages;
};

#endif // QUATERNIONROOM_H
