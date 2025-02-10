/******************************************************************************
 * SPDX-FileCopyrightText: 2018-2019 QMatrixClient Project
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "orderbytag.h"

#include "roomlistmodel.h"

#include <Quotient/ranges_extras.h>
#include <Quotient/settings.h>

static const auto Invite = RoomGroup::SystemPrefix + "invite";
static const auto DirectChat = RoomGroup::SystemPrefix + "direct";
static const auto Untagged = RoomGroup::SystemPrefix + "none";
static const auto Left = RoomGroup::SystemPrefix + "left";

// TODO: maybe move the tr() strings below from RoomListModel context
static auto InvitesLabel()
{
    return RoomListModel::tr("Invited", "The caption for invitations");
}
static auto FavouritesLabel() { return RoomListModel::tr("Favourites"); }
static auto LowPriorityLabel() { return RoomListModel::tr("Low priority"); }
static auto ServerNoticeLabel() { return RoomListModel::tr("Server notices"); }
static auto DirectChatsLabel()
{
    return RoomListModel::tr("People", "The caption for direct chats");
}
static auto UngroupedRoomsLabel()
{
    return RoomListModel::tr("Ungrouped rooms");
}
static auto LeftLabel()
{
    return RoomListModel::tr("Left", "The caption for left rooms");
}

QString tagToCaption(const QString& tag)
{
    // clang-format off
    return
        tag == Quotient::FavouriteTag ? FavouritesLabel() :
        tag == Quotient::LowPriorityTag ? LowPriorityLabel() :
        tag == Quotient::ServerNoticeTag ? ServerNoticeLabel() :
        tag.startsWith("u.") ? tag.mid(2) :
        tag;
    // clang-format on
}

QString captionToTag(const QString& caption)
{
    // clang-format off
    return
        caption == FavouritesLabel() ? Quotient::FavouriteTag :
        caption == LowPriorityLabel() ? Quotient::LowPriorityTag :
        caption == ServerNoticeLabel() ? Quotient::ServerNoticeTag :
        caption.startsWith("m.") || caption.startsWith("u.") ? caption :
        "u." + caption;
    // clang-format on
}

auto findIndexWithWildcards(const QStringList& list, const QString& value)
{
    if (list.empty() || value.isEmpty())
        return list.size();

    auto i = Quotient::findIndex(list, value);
    // Try namespace groupings (".*" in the list), from right to left
    for (QStringList::size_type dotPos = 0;
         i == list.size() && (dotPos = value.lastIndexOf('.', --dotPos)) != -1;) {
        i = Quotient::findIndex(list, value.left(dotPos + 1) + '*');
    }
    return i;
}

QVariant OrderByTag::groupLabel(const RoomGroup& g) const
{
    // clang-format off
    const auto caption =
        g.key == Untagged ? UngroupedRoomsLabel() :
        g.key == Invite ? InvitesLabel() :
        g.key == DirectChat ? DirectChatsLabel() :
        g.key == Left ? LeftLabel() :
        tagToCaption(g.key.toString());
    // clang-format on
    return RoomListModel::tr("%1 (%Ln room(s))", "", g.rooms.size()).arg(caption);
}

bool OrderByTag::groupLessThan(const QVariant& g1key, const QVariant& g2key) const
{
    const auto& lkey = g1key.toString();
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
    if (o2.has_value() != o1.has_value())
        return !o2.has_value();

    if (o1 && o2)
    {
        // Compare floats; fallthrough if neither is smaller
        if (*o1 < *o2)
            return true;

        if (*o1 > *o2)
            return false;
    }

    // 2. Neither tag order is less than the other; compare room display names
    if (auto roomCmpRes = r1->displayName().localeAwareCompare(r2->displayName()))
        return roomCmpRes < 0;

    // 3. Within the same display name, order by room id
    // (typically the case when both display names are completely empty)
    if (auto roomIdCmpRes = r1->id().compare(r2->id()))
        return roomIdCmpRes < 0;

    // 4. Room ids are equal; order by connections (=userids)
    const auto c1 = r1->connection();
    const auto c2 = r2->connection();
    if (c1 != c2)
    {
        if (auto usersCmpRes = c1->userId().compare(c2->userId()))
            return usersCmpRes < 0;

        // 4a. Two logins under the same userid: pervert, but technically correct
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

    auto tags = getFilteredTags(room);
    if (tags.empty())
        tags.push_back(Untagged);
    // Check successors, reusing room as the current frame, and for each group
    // shadow this room if there's already any of its successors in the group
    while ((room = room->successor(Quotient::JoinState::Join))) {
        auto successorTags = getFilteredTags(room);

        if (successorTags.empty())
            tags.removeOne(Untagged);
        else
            for (const auto& t: successorTags)
                if (tags.contains(t))
                    tags.removeOne(t);
        if (tags.empty())
            return {}; // No remaining groups, hide the room
    }
    groups_t vl; vl.reserve(tags.size());
    std::ranges::copy(tags, std::back_inserter(vl));
    return vl;
}

void OrderByTag::connectSignals(Connection* connection)
{
    using DCMap = Quotient::DirectChatsMap;
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

void OrderByTag::updateGroups(Room* room)
{
    AbstractRoomOrdering::updateGroups(room);

    // As the room may shadow predecessors, need to update their groups too.
    if (auto* predRoom = room->predecessor(Quotient::JoinState::Join))
        updateGroups(predRoom);
}

QStringList OrderByTag::getFilteredTags(const Room* room) const
{
    auto allTags = room->tags().keys();
    if (room->isDirectChat())
        allTags.push_back(DirectChat);

    QStringList result;
    for (const auto& t: allTags)
        if (findIndexWithWildcards(tagsOrder, '-' + t) == tagsOrder.size())
            result.push_back(t); // Only copy tags that are not disabled
    return result;
}

QStringList OrderByTag::initTagsOrder()
{
    static const QStringList DefaultTagsOrder { Invite,
                                                Quotient::FavouriteTag,
                                                QStringLiteral("u.*"),
                                                DirectChat,
                                                Untagged,
                                                Quotient::LowPriorityTag,
                                                Left };

    static const auto SettingsKey = QStringLiteral("tags_order");
    static Quotient::SettingsGroup sg { "UI/RoomsDock" };
    auto savedOrder = sg.get<QStringList>(SettingsKey);
    if (savedOrder.isEmpty())
    {
        sg.setValue(SettingsKey, DefaultTagsOrder);
        return DefaultTagsOrder;
    }
    { // Check that the order doesn't use the old prefix and migrate if it does.
        bool migrated = false;
        for (auto& s : savedOrder)
            if (s.startsWith(RoomGroup::LegacyPrefix)) {
                s.replace(0, RoomGroup::LegacyPrefix.size(),
                          RoomGroup::SystemPrefix);
                migrated = true;
            }
        if (migrated)
            sg.setValue(SettingsKey, savedOrder);
    }

    return savedOrder;
}
