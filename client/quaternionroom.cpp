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

#include "quaternionroom.h"

#include <user.h>
#include <events/roommessageevent.h>
#include <QtCore/QRegularExpression>

using namespace Quotient;

QuaternionRoom::QuaternionRoom(Connection* connection, QString roomId,
                               JoinState joinState)
    : Room(connection, std::move(roomId), joinState)
{
    connect( this, &QuaternionRoom::notificationCountChanged, this, &QuaternionRoom::countChanged );
    connect( this, &QuaternionRoom::highlightCountChanged, this, &QuaternionRoom::countChanged );
}

const QString& QuaternionRoom::cachedInput() const
{
    return m_cachedInput;
}

void QuaternionRoom::setCachedInput(const QString& input)
{
    m_cachedInput = input;
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
    return firstDisplayedMarker() == timelineEdge() ? 0 :
                firstDisplayedMarker() - messageEvents().rbegin();
}

int QuaternionRoom::savedBottomVisibleIndex() const
{
    return lastDisplayedMarker() == timelineEdge() ? 0 :
                lastDisplayedMarker() - messageEvents().rbegin();
}

void QuaternionRoom::saveViewport(int topIndex, int bottomIndex)
{
    if (topIndex == -1 || bottomIndex == -1 ||
            (bottomIndex == savedBottomVisibleIndex() &&
             (bottomIndex == 0 || topIndex == savedTopVisibleIndex())))
        return;
    if (bottomIndex == 0)
    {
        qDebug() << "Saving viewport as the latest available";
        setFirstDisplayedEventId({}); setLastDisplayedEventId({});
        return;
    }
    qDebug() << "Saving viewport:" << topIndex << "thru" << bottomIndex;
    setFirstDisplayedEvent(maxTimelineIndex() - topIndex);
    setLastDisplayedEvent(maxTimelineIndex() - bottomIndex);
}

QString QuaternionRoom::prettyPrint(const QString& plainText) const
{
    return Room::prettyPrint(plainText);
}

bool QuaternionRoom::canSwitchVersions() const
{
    return Room::canSwitchVersions();
}

QString QuaternionRoom::safeMemberName(const QString& userId) const
{
    return sanitized(roomMembername(userId));
}

void QuaternionRoom::countChanged()
{
    if( displayed() && !hasUnreadMessages() )
    {
        resetNotificationCount();
        resetHighlightCount();
    }
}

void QuaternionRoom::onAddNewTimelineEvents(timeline_iter_t from)
{
    std::for_each(from, messageEvents().cend(),
                  [this] (const TimelineItem& ti) { checkForHighlights(ti); });
}

void QuaternionRoom::onAddHistoricalTimelineEvents(rev_iter_t from)
{
    std::for_each(from, messageEvents().crend(),
                  [this] (const TimelineItem& ti) { checkForHighlights(ti); });
}

void QuaternionRoom::checkForHighlights(const Quotient::TimelineItem& ti)
{
    auto localUserId = localUser()->id();
    if (ti->senderId() == localUserId)
        return;
    if (auto* e = ti.viewAs<RoomMessageEvent>())
    {
        const QRegularExpression localUserRe("(\\W|^)" + localUserId + "(\\W|$)", QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
        const QRegularExpression roomMembernameRe("(\\W|^)" + roomMembername(localUserId) + "(\\W|$)", QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
        const auto& text = e->plainBody();
        QRegularExpressionMatch localMatch = localUserRe.match(text, 0, QRegularExpression::PartialPreferFirstMatch);
        QRegularExpressionMatch roomMemberMatch = roomMembernameRe.match(text, 0, QRegularExpression::PartialPreferFirstMatch);
        if (localMatch.hasMatch() ||
                roomMemberMatch.hasMatch())
            highlights.insert(e);
    }
}
