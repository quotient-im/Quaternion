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

#include "lib/user.h"
#include "lib/events/roommessageevent.h"

using namespace QMatrixClient;

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

void QuaternionRoom::checkForHighlights(const QMatrixClient::TimelineItem& ti)
{
    auto localUserId = localUser()->id();
    if (ti->senderId() == localUserId)
        return;
    if (ti->type() == EventType::RoomMessage)
    {
        const auto& text = ti.viewAs<RoomMessageEvent>()->plainBody();
        if (text.contains(localUserId) ||
                text.contains(roomMembername(localUserId)))
            highlights.insert(ti.event());
    }
}
