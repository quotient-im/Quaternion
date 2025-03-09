/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2016 Felix Rohrbach <kde@fxrh.de>              *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#include "quaternionroom.h"

#include "logging_categories.h"

#include <Quotient/events/roommessageevent.h>

#include <Quotient/user.h>

#include <QtCore/QRegularExpression>
#include <QtGui/QTextDocumentFragment>

#include <ranges>

using namespace Quotient;

QuaternionRoom::QuaternionRoom(Connection* connection, QString roomId, JoinState joinState)
    : Room(connection, std::move(roomId), joinState)
{}

const QString& QuaternionRoom::cachedUserFilter() const
{
    return m_cachedUserFilter;
}

void QuaternionRoom::setCachedUserFilter(const QString& input)
{
    m_cachedUserFilter = input;
}

bool QuaternionRoom::isEventHighlighted(const RoomEvent* e) const
{
    return highlights.contains(e);
}

int QuaternionRoom::savedTopVisibleIndex() const
{
    return firstDisplayedMarker() == historyEdge() ? 0 :
                firstDisplayedMarker() - messageEvents().rbegin();
}

int QuaternionRoom::savedBottomVisibleIndex() const
{
    return lastDisplayedMarker() == historyEdge() ? 0 :
                lastDisplayedMarker() - messageEvents().rbegin();
}

void QuaternionRoom::saveViewport(int topIndex, int bottomIndex, bool force)
{
    // Don't save more frequently than once a second
    static auto lastSaved = QDateTime::currentMSecsSinceEpoch();
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (!force && lastSaved >= now - 1000)
        return;
    lastSaved = now;

    if (topIndex == -1 || bottomIndex == -1
        || (bottomIndex == savedBottomVisibleIndex()
            && (bottomIndex == 0 || topIndex == savedTopVisibleIndex())))
        return;
    if (bottomIndex == 0) {
        qCDebug(MAIN) << "Saving viewport as the latest available";
        setFirstDisplayedEventId({});
        setLastDisplayedEventId({});
        return;
    }
    qCDebug(MAIN) << "Saving viewport:" << topIndex << "thru" << bottomIndex;
    setFirstDisplayedEvent(maxTimelineIndex() - topIndex);
    setLastDisplayedEvent(maxTimelineIndex() - bottomIndex);
}

bool QuaternionRoom::canRedact(const Quotient::EventId& eventId) const
{
    if (const auto it = findInTimeline(eventId); it != historyEdge()) {
        const auto localMemberId = localMember().id();
        const auto memberId = it->event()->senderId();
        if (localMemberId == memberId)
            return true;

        const auto& ple = currentState().get<RoomPowerLevelsEvent>();
        const auto currentUserPl = ple->powerLevelForUser(localMemberId);
        return currentUserPl >= ple->redact() && currentUserPl >= ple->powerLevelForUser(memberId);
    }
    return false;
}

void QuaternionRoom::onGettingSingleEvent(const QString& evtId)
{
    std::erase_if(singleEventRequests, [this, evtId](SingleEventRequest& r) {
        const bool idMatches = r.eventId == evtId;
        if (idMatches) {
            if (!r.requestHandle.isFinished())
                r.requestHandle.abandon();
            std::ranges::for_each(r.eventIdsToRefresh, std::bind_front(&Room::updatedEvent, this));
        }
        return idMatches;
    });
}

const RoomEvent* QuaternionRoom::getSingleEvent(const QString& eventId, const QString& originEventId)
{
    if (auto timelineIt = findInTimeline(eventId); timelineIt != historyEdge())
        return timelineIt->event();
    if (auto cachedIt = cachedEvents.find(eventId); cachedIt != cachedEvents.cend())
        return cachedIt->second.get();

    auto requestIt = std::ranges::find(singleEventRequests, eventId, &SingleEventRequest::eventId);
    if (requestIt == singleEventRequests.cend())
        requestIt = singleEventRequests.insert(
            requestIt,
            { eventId, connection()
                           ->callApi<GetOneRoomEventJob>(id(), eventId)
                           .then([this](RoomEventPtr&& pEvt) {
                               if (pEvt == nullptr) {
                                   qCCritical(MAIN, "/rooms/event returned an empty event");
                                   return;
                               }
                               const auto [it, cachedEventInserted] =
                                   cachedEvents.insert_or_assign(pEvt->id(), std::move(pEvt));
                               const auto evtId = it->first;
                               if (QUO_ALARM(!cachedEventInserted))
                                   emit updatedEvent(evtId); // At least notify clients...
                               onGettingSingleEvent(evtId);
                           }) });
    requestIt->eventIdsToRefresh.push_back(originEventId);

    return nullptr;
}

void QuaternionRoom::onAddNewTimelineEvents(timeline_iter_t from)
{
    std::for_each(from, messageEvents().cend(),
                  std::bind_front(&QuaternionRoom::checkForHighlights, this));
}

void QuaternionRoom::onAddHistoricalTimelineEvents(rev_iter_t from)
{
    std::for_each(from, messageEvents().crend(),
                  std::bind_front(&QuaternionRoom::checkForHighlights, this));
    checkForRequestedEvents(from);
}

void QuaternionRoom::checkForHighlights(const Quotient::TimelineItem& ti)
{
    const auto localUserId = localMember().id();
    if (ti->senderId() == localUserId)
        return;
    if (auto* e = ti.viewAs<RoomMessageEvent>()) {
        constexpr auto ReOpt = QRegularExpression::MultilineOption
                               | QRegularExpression::CaseInsensitiveOption;
        constexpr auto MatchOpt = QRegularExpression::PartialPreferFirstMatch;

        // Building a QRegularExpression is quite expensive and this function is called a lot
        // Given that the localUserId is usually the same we can reuse the QRegularExpression instead of building it every time
        static QHash<QString, QRegularExpression> localUserExpressions;
        static QHash<QString, QRegularExpression> roomMemberExpressions;

        if (!localUserExpressions.contains(localUserId)) {
            localUserExpressions[localUserId] = QRegularExpression("(\\W|^)" + localUserId + "(\\W|$)", ReOpt);
        }

        const auto memberName = member(localUserId).disambiguatedName();
        if (!roomMemberExpressions.contains(memberName)) {
            // FIXME: unravels if the room member name contains characters special
            //        to regexp($, e.g.)
            roomMemberExpressions[memberName] =
                QRegularExpression("(\\W|^)" + memberName + "(\\W|$)", ReOpt);
        }

        const auto& text = e->plainBody();
        const auto& localMatch = localUserExpressions[localUserId].match(text, 0, MatchOpt);
        const auto& roomMemberMatch = roomMemberExpressions[memberName].match(text, 0, MatchOpt);
        if (localMatch.hasMatch() || roomMemberMatch.hasMatch())
            highlights.insert(e);
    }
}

QuaternionRoom::EventFuture QuaternionRoom::ensureHistory(const QString& upToEventId,
                                                          quint16 maxWaitSeconds)
{
    if (auto eventIt = findInTimeline(upToEventId); eventIt != historyEdge())
        return makeReadyVoidFuture();

    if (allHistoryLoaded())
        return {};
    // Request a small number of events (or whatever the ongoing request says, if there's any),
    // to make sure checkForRequestedEvents() gets executed
    getPreviousContent();
    HistoryRequest r{ upToEventId,
                      QDeadlineTimer{ std::chrono::seconds(maxWaitSeconds), Qt::VeryCoarseTimer } };
    auto future = r.promise.future();
    r.promise.start();
    historyRequests.push_back(std::move(r));
    return future;
}

namespace {
using namespace std::ranges;
template <typename RangeT>
    requires(std::convertible_to<range_reference_t<RangeT>, QString>)
inline auto dumpJoined(const RangeT& range, const QString& separator = u","_s)
{
    return
#if defined(__cpp_lib_ranges_join_with) && defined(__cpp_lib_ranges_to_container)
        to<QString>(join_with_view(range, separator));
#else
        QStringList(begin(range), end(range)).join(separator);
#endif
}
}

void QuaternionRoom::checkForRequestedEvents(const rev_iter_t& from)
{
    using namespace std::ranges;
    const auto addedRange = subrange(from, historyEdge());
    for (const auto& evtId : transform_view(addedRange, &RoomEvent::id)) {
        cachedEvents.erase(evtId);
        onGettingSingleEvent(evtId);
    }
    std::erase_if(historyRequests, [this, addedRange](HistoryRequest& request) {
        auto& [upToEventId, deadline, promise] = request;
        if (promise.isCanceled()) {
            qCInfo(MAIN) << "The request to ensure event" << upToEventId << "has been cancelled";
            return true;
        }
        if (auto it = find(addedRange, upToEventId, &RoomEvent::id); it != historyEdge()) {
            promise.finish();
            return true;
        }
        if (deadline.hasExpired()) {
            qCWarning(MAIN) << "Timeout - giving up on obtaining event" << upToEventId;
            promise.future().cancel();
            return true;
        }
        return false;
    });
    if (!historyRequests.empty()) {
        auto requestedIds =
            dumpJoined(transform_view(historyRequests, &HistoryRequest::upToEventId));
        if (allHistoryLoaded()) {
            qCDebug(MAIN).noquote() << "Could not find in the whole room history:" << requestedIds;
            for_each(historyRequests, [](auto& r) { r.promise.future().cancel(); });
            historyRequests.clear();
        }
        static constexpr auto EventsProgression = std::array{ 50, 100, 200, 500, 1000 };
        static_assert(is_sorted(EventsProgression));
        const auto thisMany = requestedHistorySize() >= EventsProgression.back()
                                  ? EventsProgression.back()
                                  : *upper_bound(EventsProgression, requestedHistorySize());
        qCDebug(MAIN).noquote() << "Requesting" << thisMany << "events, looking for"
                                << requestedIds;
        getPreviousContent(thisMany);
    }
}

void QuaternionRoom::sendMessage(const QTextDocumentFragment& richText,
                                 HtmlFilter::Options htmlFilterOptions)
{
    const auto& plainText = richText.toPlainText();
    const auto& html = HtmlFilter::toMatrixHtml(richText.toHtml(), { this }, htmlFilterOptions);
    Q_ASSERT(!plainText.isEmpty());
    // Send plain text if htmlText has no markup or just <br/> elements
    // (those are easily represented as line breaks in plain text)
    using namespace Quotient;
    static const QRegularExpression MarkupRE{ "<(?![Bb][Rr])"_L1 };
    // TODO: use Room::postText() once we're on lib 0.9.3+
    post<RoomMessageEvent>(plainText, MessageEventType::Text,
                           html.contains(MarkupRE)
                               ? std::make_unique<EventContent::TextContent>(html, u"text/html"_s)
                               : nullptr);
}
