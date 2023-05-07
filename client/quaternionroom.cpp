/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2016 Felix Rohrbach <kde@fxrh.de>              *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#include "quaternionroom.h"

#include <Quotient/user.h>
#include <Quotient/events/roommessageevent.h>
#include <QtCore/QRegularExpression>

using namespace Quotient;

QuaternionRoom::QuaternionRoom(Connection* connection, QString roomId,
                               JoinState joinState)
    : Room(connection, std::move(roomId), joinState)
{
    connect(this, &Room::namesChanged,
            this, &QuaternionRoom::htmlSafeDisplayNameChanged);
}

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
        qDebug() << "Saving viewport as the latest available";
        setFirstDisplayedEventId({});
        setLastDisplayedEventId({});
        return;
    }
    qDebug() << "Saving viewport:" << topIndex << "thru" << bottomIndex;
    setFirstDisplayedEvent(maxTimelineIndex() - topIndex);
    setLastDisplayedEvent(maxTimelineIndex() - bottomIndex);
}

QString QuaternionRoom::htmlSafeDisplayName() const
{
    return displayName().toHtmlEscaped();
}

void QuaternionRoom::onAddNewTimelineEvents(timeline_iter_t from)
{
    std::for_each(from, messageEvents().cend(),
                  [this](const TimelineItem& ti) { checkForHighlights(ti); });
}

void QuaternionRoom::onAddHistoricalTimelineEvents(rev_iter_t from)
{
    std::for_each(from, messageEvents().crend(),
                  [this](const TimelineItem& ti) { checkForHighlights(ti); });
}

void QuaternionRoom::checkForHighlights(const Quotient::TimelineItem& ti)
{
    const auto localUserId = localUser()->id();
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

        const auto memberName = disambiguatedMemberName(localUserId);
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
