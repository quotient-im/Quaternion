/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2016 Felix Rohrbach <kde@fxrh.de>              *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#pragma once

#include "htmlfilter.h"

#include <Quotient/csapi/rooms.h>

#include <Quotient/room.h>

#include <QtCore/QDeadlineTimer>

class QTextDocumentFragment;

class QuaternionRoom: public Quotient::Room
{
    Q_OBJECT
public:
    using RoomEvent = Quotient::RoomEvent;

    QuaternionRoom(Quotient::Connection* connection, QString roomId,
                   Quotient::JoinState joinState);

    const QString& cachedUserFilter() const;
    void setCachedUserFilter(const QString& input);

    bool isEventHighlighted(const Quotient::RoomEvent* e) const;

    Q_INVOKABLE int savedTopVisibleIndex() const;
    Q_INVOKABLE int savedBottomVisibleIndex() const;
    Q_INVOKABLE void saveViewport(int topIndex, int bottomIndex, bool force = false);

    bool canRedact(const Quotient::EventId& eventId) const;

    using EventFuture = QFuture<void>;

    //! \brief Loads the message history until the specified event id is found
    //!
    //! This is potentially heavy; use it sparingly. One intended use case is loading the timeline
    //! until the last read event, assuming that the last read event is not too far back and that
    //! the user will read or at least scroll through the just loaded events anyway. This will not
    //! be necessary once we move to sliding sync but sliding sync support is still a bit away in
    //! the future.
    //!
    //! Because the process is heavy (particularly on the homeserver), ensureHistory() will cancel
    //! after \p maxWaitSeconds.
    //! \return the future that resolves to the event with \p eventId, or self-cancels if the event
    //!         is not found
    Q_INVOKABLE EventFuture ensureHistory(const QString& upToEventId, quint16 maxWaitSeconds = 20);

    //! \brief Obtain an arbitrary room event by its id that is available locally
    //!
    Q_INVOKABLE const Quotient::RoomEvent* getSingleEvent(const QString& eventId,
                                                          const QString& originEventId);

    void sendMessage(const QTextDocumentFragment& richText,
                     HtmlFilter::Options htmlFilterOptions = HtmlFilter::Default);

private:
    using EventPromise = QPromise<void>;
    using EventId = Quotient::EventId;

    struct HistoryRequest {
        EventId upToEventId;
        QDeadlineTimer deadline;
        EventPromise promise{};
    };
    std::vector<HistoryRequest> historyRequests;

    struct SingleEventRequest {
        EventId eventId;
        Quotient::JobHandle<Quotient::GetOneRoomEventJob> requestHandle;
        std::vector<QString> eventIdsToRefresh{};
    };
    std::vector<SingleEventRequest> singleEventRequests;
    std::unordered_map<EventId, Quotient::event_ptr_tt<const RoomEvent>> cachedEvents;

    QSet<const RoomEvent*> highlights;
    QString m_cachedUserFilter;

    void onAddNewTimelineEvents(timeline_iter_t from) override;
    void onAddHistoricalTimelineEvents(rev_iter_t from) override;

    void checkForHighlights(const Quotient::TimelineItem& ti);
    void checkForRequestedEvents(const rev_iter_t& from);
    void onGettingSingleEvent(const QString& evtId);
};
