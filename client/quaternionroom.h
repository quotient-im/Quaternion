/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2016 Felix Rohrbach <kde@fxrh.de>              *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#pragma once

#include <Quotient/room.h>

class QuaternionRoom: public Quotient::Room
{
        Q_OBJECT
        Q_PROPERTY(QString htmlSafeDisplayName READ htmlSafeDisplayName NOTIFY htmlSafeDisplayNameChanged)
        Q_PROPERTY(int requestedEventsCount READ requestedEventsCount NOTIFY requestedEventsCountChanged)
    public:
        QuaternionRoom(Quotient::Connection* connection,
                       QString roomId, Quotient::JoinState joinState);

        const QString& cachedUserFilter() const;
        void setCachedUserFilter(const QString& input);

        bool isEventHighlighted(const Quotient::RoomEvent* e) const;

        Q_INVOKABLE int savedTopVisibleIndex() const;
        Q_INVOKABLE int savedBottomVisibleIndex() const;
        Q_INVOKABLE void saveViewport(int topIndex, int bottomIndex,
                                      bool force = false);

        QString htmlSafeDisplayName() const;
        int requestedEventsCount() const;

    public slots:
        // TODO, 0.0.96: move logic to libQuotient 0.9 and get rid of it here
        void getHistory(int limit);

    signals:
        // Gotta wrap the Room::namesChanged signal because it has parameters
        // and moc cannot use signals with parameters defined in the parent
        // class as NOTIFY targets
        void htmlSafeDisplayNameChanged();
        // TODO, 0.0.96: same as for getHistory()
        void requestedEventsCountChanged();

    private:
        QSet<const Quotient::RoomEvent*> highlights;
        QString m_cachedUserFilter;
        int m_requestedEventsCount = 0;

        void onAddNewTimelineEvents(timeline_iter_t from) override;
        void onAddHistoricalTimelineEvents(rev_iter_t from) override;

        void checkForHighlights(const Quotient::TimelineItem& ti);
};
