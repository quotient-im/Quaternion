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

#include <room.h>

class QuaternionRoom: public QMatrixClient::Room
{
        Q_OBJECT
    public:
        QuaternionRoom(QMatrixClient::Connection* connection,
                       QString roomId, QMatrixClient::JoinState joinState);

        const QString& cachedInput() const;
        void setCachedInput(const QString& input);

        bool isEventHighlighted(const QMatrixClient::RoomEvent* e) const;

        Q_INVOKABLE int savedTopVisibleIndex() const;
        Q_INVOKABLE int savedBottomVisibleIndex() const;
        Q_INVOKABLE void saveViewport(int topIndex, int bottomIndex);

    private slots:
        void countChanged();

    private:
        QSet<const QMatrixClient::RoomEvent*> highlights;
        QString m_cachedInput;

        void onAddNewTimelineEvents(timeline_iter_t from) override;
        void onAddHistoricalTimelineEvents(rev_iter_t from) override;

        void checkForHighlights(const QMatrixClient::TimelineItem& ti);
};
