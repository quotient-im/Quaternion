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

class QuaternionRoom: public Quotient::Room
{
        Q_OBJECT
    public:
        QuaternionRoom(Quotient::Connection* connection,
                       QString roomId, Quotient::JoinState joinState);

        const QString& cachedInput() const;
        void setCachedInput(const QString& input);

        const QString& cachedUserFilter() const;
        void setCachedUserFilter(const QString& input);

        bool isEventHighlighted(const Quotient::RoomEvent* e) const;

        Q_INVOKABLE int savedTopVisibleIndex() const;
        Q_INVOKABLE int savedBottomVisibleIndex() const;
        Q_INVOKABLE void saveViewport(int topIndex, int bottomIndex);
        // FIXME: This should be deleted as soon as Room::prettyPrint and
        // Room::canSwitchVersions() become Q_INVOKABLE (canSwitchVersions()
        // may - or may not - become a Q_PROPERTY even).
        Q_INVOKABLE QString prettyPrint(const QString& plainText) const;
        Q_INVOKABLE bool canSwitchVersions() const;

        /*! Get a display-safe member name in the context of this room
         * Display-safe means HTML-safe + without RLO/LRO markers (see #545)
         */
        Q_INVOKABLE QString safeMemberName(const QString& userId) const;

    private slots:
        void countChanged();

    private:
        QSet<const Quotient::RoomEvent*> highlights;
        QString m_cachedInput;
        QString m_cachedUserFilter;

        void onAddNewTimelineEvents(timeline_iter_t from) override;
        void onAddHistoricalTimelineEvents(rev_iter_t from) override;

        void checkForHighlights(const Quotient::TimelineItem& ti);
};
