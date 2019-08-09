/******************************************************************************
 * Copyright (C) 2018-2019 QMatrixClient Project
 *
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
 */

#include "orderbytag.h"

#include "roomlistmodel.h"

#include <settings.h>

const QString Invite = QStringLiteral("org.qmatrixclient.invite");
const QString DirectChat = QStringLiteral("org.qmatrixclient.direct");
const QString Untagged = QStringLiteral("org.qmatrixclient.none");
const QString Left = QStringLiteral("org.qmatrixclient.left");

template <typename LT, typename VT>
inline auto findIndex(const QList<LT>& list, const VT& value)
{
    // Using std::find() instead of indexOf() so that not found keys were
    // naturally sorted after found ones (index == list.end() - list.begin()
    // is more than any index in the list, while index == -1 is less).
    return std::find(list.begin(), list.end(), value) - list.begin();
}

auto findIndexWithWildcards(const QStringList& list, const QString& value)
{
    if (list.empty() || value.isEmpty())
        return list.size();

    auto i = findIndex(list, value);
    // Try namespace groupings (".*" in the list), from right to left
    for (int dotPos = 0;
         i == list.size() && (dotPos = value.lastIndexOf('.', --dotPos)) != -1;)
    {
        i = findIndex(list, value.left(dotPos + 1) + '*');
    }
    return i;
}

QVariant OrderByTag::groupLabel(const RoomGroup& g) const
{
    // TODO: maybe move the tr() strings below from RoomListModel context
    static const auto InvitesLabel =
            RoomListModel::tr("Invited", "The caption for invitations");
    static const auto FavouritesLabel = RoomListModel::tr("Favourites");
    static const auto LowPriorityLabel = RoomListModel::tr("Low priority");
    static const auto DirectChatsLabel =
            RoomListModel::tr("People", "The caption for direct chats");
    static const auto UngroupedRoomsLabel = RoomListModel::tr("Ungrouped rooms");
    static const auto LeftLabel =
            RoomListModel::tr("Left", "The caption for left rooms");

    const auto caption =
            g.key == Untagged ? UngroupedRoomsLabel :
            g.key == Invite ? InvitesLabel :
            g.key == DirectChat ? DirectChatsLabel :
            g.key == Left ? LeftLabel :
            g.key == Quotient::FavouriteTag ? FavouritesLabel :
            g.key == Quotient::LowPriorityTag ? LowPriorityLabel :
            g.key.toString().startsWith("u.") ? g.key.toString().mid(2) :
            g.key.toString();
    return RoomListModel::tr("%1 (%Ln room(s))", "", g.rooms.size()).arg(caption);
}

bool OrderByTag::groupLessThan(const RoomGroup& g1, const QVariant& g2key) const
{
    const auto& lkey = g1.key.toString();
    const auto& rkey = g2key.toString();
    // See above
    auto li = findIndexWithWildcards(tagsOrder, lkey);
    auto ri = findIndexWithWildcards(tagsOrder, rkey);
    return li < ri || (li == ri && lkey < rkey);
}

bool OrderByTag::roomLessThan(const QVariant& groupKey,
                              const Room* r1, const Room* r2) const
{
    if (r1 == r2)
        return false; // 0. Short-circuit for coinciding room objects

    // 1. Compare tag order values
    const auto& tag = groupKey.toString();
    auto o1 = r1->tag(tag).order;
    auto o2 = r2->tag(tag).order;
    if (o2.omitted() != o1.omitted())
        return o2.omitted();

    if (!o1.omitted() && !o2.omitted())
    {
        // Compare floats; fallthrough if neither is smaller
        if (o1.value() < o2.value())
            return true;

        if (o1.value() > o2.value())
            return false;
    }

    // 2. Neither tag order is less than the other; compare room display names
    if (auto roomCmpRes = r1->displayName().localeAwareCompare(r2->displayName()))
        return roomCmpRes < 0;

    // 4. Within the same display name, order by room id
    // (typically the case when both display names are completely empty)
    if (auto roomIdCmpRes = r1->id().compare(r2->id()))
        return roomIdCmpRes < 0;

    // 3. Room ids are equal; order by connections (=userids)
    const auto c1 = r1->connection();
    const auto c2 = r2->connection();
    if (c1 != c2)
    {
        if (auto usersCmpRes = c1->userId().compare(c2->userId()))
            return usersCmpRes < 0;

        // 3a. Two logins under the same userid: pervert, but technically correct
        Q_ASSERT(c1->accessToken() != c2->accessToken());
        return c1->accessToken() < c2->accessToken();
    }

    // 5. Assume two incarnations of the room with the different join state
    // (by design, join states are distinct within one connection+roomid)
    Q_ASSERT(r1->joinState() != r2->joinState());
    return r1->joinState() < r2->joinState();
}

AbstractRoomOrdering::groups_t OrderByTag::roomGroups(const Room* room) const
{
    if (room->joinState() == Quotient::JoinState::Invite)
        return groups_t {{ Invite }};
    if (room->joinState() == Quotient::JoinState::Leave)
        return groups_t {{ Left }};
    auto tags = room->tags().keys();
    groups_t vl; vl.reserve(tags.size());
    std::copy(tags.cbegin(), tags.cend(), std::back_inserter(vl));
    if (room->isDirectChat())
        vl.push_back(DirectChat);
    if (vl.empty())
        vl.push_back(Untagged);
    return vl;
}

void OrderByTag::connectSignals(Connection* connection)
{
    using DCMap = Connection::DirectChatsMap;
    connect( connection, &Connection::directChatsListChanged, this,
        [this,connection] (const DCMap& additions, const DCMap& removals) {
            // The same room may show up in removals and in additions if it
            // moves from one userid to another (pretty weird but encountered
            // in the wild). Therefore process removals first.
            for (const auto& rId: removals)
                if (auto* r = connection->room(rId))
                    updateGroups(r);
            for (const auto& rId: additions)
                if (auto* r = connection->room(rId))
                    updateGroups(r);
        });
}

void OrderByTag::connectSignals(Room* room)
{
    connect(room, &Room::displaynameChanged,
            this, [this,room] { updateGroups(room); });
    connect(room, &Room::tagsChanged,
            this, [this,room] { updateGroups(room); });
    connect(room, &Room::joinStateChanged,
            this, [this,room] { updateGroups(room); });
}

QStringList OrderByTag::initTagsOrder()
{
    using namespace Quotient;
    static const QStringList DefaultTagsOrder {
        Invite, FavouriteTag, QStringLiteral("u.*"), DirectChat, Untagged,
        LowPriorityTag, Left
    };

    static const auto SettingsKey = QStringLiteral("tags_order");
    static Quotient::SettingsGroup sg { "UI/RoomsDock" };
    const auto savedOrder = sg.get<QStringList>(SettingsKey);
    if (savedOrder.isEmpty())
    {
        sg.setValue(SettingsKey, DefaultTagsOrder);
        return DefaultTagsOrder;
    }
    return savedOrder;
}
