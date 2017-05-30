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

#pragma once

#include "message.h"
#include "lib/room.h"

class QuaternionRoom: public QMatrixClient::Room
{
        Q_OBJECT
    public:
        using Timeline = QVector<Message>;

        QuaternionRoom(QMatrixClient::Connection* connection, QString roomId);

        /**
         * set/get whether this room is currently show to the user.
         * This is used to mark messages as read.
         */
        void setShown(bool shown);
        bool isShown();

        void setCachedInput(const QString& input);
        const QString& cachedInput() const;

        const Timeline& messages() const;

        /** Converts all that looks like a URL into HTML links */
        void linkifyUrls(QString& text) const;

        /** Returns the passed plain text pretty-printed in HTML */
        QString prettyPrint(const QString& plainText) const;

    protected:
        void doAddNewMessageEvents(const QMatrixClient::RoomEvents& events) override;
        void doAddHistoricalMessageEvents(const QMatrixClient::RoomEvents& events) override;

    private slots:
        void countChanged();

    private:
        Timeline m_messages;
        bool m_shown;
        QString m_cachedInput;
};
