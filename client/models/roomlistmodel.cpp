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

#include "roomlistmodel.h"

#include "../quaternionroom.h"

#include <user.h>
#include <connection.h>
#include <settings.h>

#include <QtGui/QIcon>
#include <QtCore/QStringBuilder>

// See the comment next to QGuiApplication::palette().brush() usage in this file
#include <QtGui/QGuiApplication>
#include <QtGui/QPalette>
#include <QtGui/QBrush>

#include <functional>

using namespace std::placeholders;

//const RoomGroups& AbstractRoomOrdering::modelGroups() const
//{
//    return model()->m_roomGroups;
//}

//RoomGroups& AbstractRoomOrdering::modelGroups()
//{
//    return model()->m_roomGroups;
//}

AbstractRoomOrdering::groupLessThan_closure_t
AbstractRoomOrdering::groupLessThanFactory() const
{
    return std::bind(&AbstractRoomOrdering::groupLessThan, this, _1, _2);
}

AbstractRoomOrdering::roomLessThan_closure_t
AbstractRoomOrdering::roomLessThanFactory(const QVariant& group) const
{
    return std::bind(&AbstractRoomOrdering::roomLessThan, this, group, _1, _2);
}

void AbstractRoomOrdering::updateGroups(Room* room)
{
    model()->updateGroups(room);
}

template <typename LT, typename VT>
inline auto findIndex(const QList<LT>& list, const VT& value)
{
    // Using std::find() instead of indexOf() so that not found keys were
    // naturally sorted after found ones.
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

const QString OrderByTag::Invite = QStringLiteral("org.qmatrixclient.invite");
const QString OrderByTag::DirectChat = QStringLiteral("org.qmatrixclient.direct");
const QString OrderByTag::Untagged = QStringLiteral("org.qmatrixclient.none");
const QString OrderByTag::Left = QStringLiteral("org.qmatrixclient.left");

QVariant OrderByTag::groupLabel(const RoomGroup& g) const
{
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
            g.key == QMatrixClient::FavouriteTag ? FavouritesLabel :
            g.key == QMatrixClient::LowPriorityTag ? LowPriorityLabel :
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
    if (room->joinState() == QMatrixClient::JoinState::Invite)
        return groups_t {{ Invite }};
    if (room->joinState() == QMatrixClient::JoinState::Leave)
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
    QObject::connect( connection, &Connection::directChatsListChanged, model(),
        [this,connection] (DCMap additions, DCMap removals) {
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
    QObject::connect(room, &Room::displaynameChanged,
                     model(), [this,room] { updateGroups(room); });
    QObject::connect(room, &Room::tagsChanged,
                     model(), [this,room] { updateGroups(room); });

    QObject::connect(room, &Room::joinStateChanged,
                     model(), [this,room] { updateGroups(room); });
}

QStringList OrderByTag::initTagsOrder()
{
    using namespace QMatrixClient;
    static const QStringList DefaultTagsOrder {
        Invite, FavouriteTag, QStringLiteral("u.*"), DirectChat, Untagged,
        LowPriorityTag, Left
    };

    static const auto SettingsKey = QStringLiteral("tags_order");
    static QMatrixClient::SettingsGroup sg { "UI/RoomsDock" };
    const auto savedOrder = sg.get<QStringList>(SettingsKey);
    if (savedOrder.isEmpty())
    {
        sg.setValue(SettingsKey, DefaultTagsOrder);
        return DefaultTagsOrder;
    }
    return savedOrder;
}

RoomListModel::RoomListModel(QObject* parent)
    : QAbstractItemModel(parent)
{
    connect(this, &RoomListModel::modelAboutToBeReset,
            this, &RoomListModel::saveCurrentSelection);
    connect(this, &RoomListModel::modelReset,
            this, &RoomListModel::restoreCurrentSelection);
}

void RoomListModel::addConnection(QMatrixClient::Connection* connection)
{
    Q_ASSERT(connection);

    using namespace QMatrixClient;
    m_connections.emplace_back(connection, this);
    connect( connection, &Connection::loggedOut,
             this, [=]{ deleteConnection(connection); } );
    connect( connection, &Connection::newRoom,
             this, &RoomListModel::addRoom);
    m_roomOrder->connectSignals(connection);

    for (auto* r: connection->roomMap())
        addRoom(r);
}

void RoomListModel::deleteConnection(QMatrixClient::Connection* connection)
{
    Q_ASSERT(connection);
    const auto connIt =
            find(m_connections.begin(), m_connections.end(), connection);
    if (connIt == m_connections.end())
    {
        Q_ASSERT_X(connIt == m_connections.end(), __FUNCTION__,
                   "Connection is missing in the rooms model");
        return;
    }

    for (auto* r: connection->roomMap())
        deleteRoom(r);
    m_connections.erase(connIt);
    connection->disconnect(this);
}

void RoomListModel::deleteTag(QModelIndex index)
{
    if (!isValidGroupIndex(index))
        return;
    const auto tag = m_roomGroups[index.row()].key.toString();
    if (tag.isEmpty())
    {
        qCritical() << "RoomListModel: Invalid tag at position" << index.row();
        return;
    }
    if (tag.startsWith("org.qmatrixclient."))
    {
        qWarning() << "RoomListModel: System groups cannot be deleted "
                      "(tried to delete" << tag << "group)";
        return;
    }
    // After the below loop, the respective group will magically disappear from
    // m_roomGroups as well due to tagsChanged() triggered from removeTag()
    for (const auto& c: m_connections)
        for (auto* r: c->roomsWithTag(tag))
            r->removeTag(tag);
}

void RoomListModel::visitRoom(const Room& room,
                              const std::function<void(QModelIndex)>& visitor)
{
    // Copy persistent indices because visitors may alter m_roomIndices
    const auto indices = m_roomIndices.values(&room);
    for (const auto& idx: indices)
    {
        Q_ASSERT(isValidRoomIndex(idx));
        if (roomAt(idx) == &room)
            visitor(idx);
        else {
            qCritical() << "Room at" << idx << "is" << roomAt(idx)->objectName()
                        << "instead of" << room.objectName();
            Q_ASSERT(false);
        }
    }
}

QVariant RoomListModel::roomGroupAt(QModelIndex idx) const
{
    Q_ASSERT(idx.isValid()); // Root item shouldn't come here
    // If we're on a room, find its group; otherwise just take the index
    const auto groupIt = m_roomGroups.cbegin() +
                (idx.parent().isValid() ? idx.parent() : idx).row();
    return groupIt != m_roomGroups.end() ? groupIt->key : QVariant();
}

QuaternionRoom* RoomListModel::roomAt(QModelIndex idx) const
{
    return isValidRoomIndex(idx) ?
            static_cast<QuaternionRoom*>(
                  m_roomGroups[idx.parent().row()].rooms[idx.row()]) : nullptr;
}

QModelIndex RoomListModel::indexOf(const QVariant& group) const
{
    const auto groupIt = lowerBoundGroup(group);
    if (groupIt == m_roomGroups.end() || groupIt->key != group)
        return {}; // Group not found
    return index(groupIt - m_roomGroups.begin(), 0);
}

QModelIndex RoomListModel::indexOf(const QVariant& group, Room* room) const
{
    auto it = m_roomIndices.find(room);
    if (group.isNull() && it != m_roomIndices.end())
        return *it;
    for (;it != m_roomIndices.end() && it.key() == room; ++it)
    {
        Q_ASSERT(isValidRoomIndex(*it));
        if (m_roomGroups[it->parent().row()].key == group)
            return *it;
    }
    return {};
}

QModelIndex RoomListModel::index(int row, int column,
                                 const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
        return {};

    // Groups get internalId() == -1, rooms get the group ordinal number
    return createIndex(row, column,
                       quintptr(parent.isValid() ? parent.row() : -1));
}

QModelIndex RoomListModel::parent(const QModelIndex& child) const
{
    const auto parentPos = int(child.internalId());
    return child.isValid() && parentPos != -1
            ? index(parentPos, 0) : QModelIndex();
}

void RoomListModel::addRoom(Room* room)
{
    addRoomToGroups(room);
    connectRoomSignals(room);
}

void RoomListModel::deleteRoom(Room* room)
{
    visitRoom(*room, std::bind(&RoomListModel::doRemoveRoom, this, _1));
    room->disconnect(this);
}

RoomGroups::iterator RoomListModel::tryInsertGroup(const QVariant& key,
                                                   bool notify)
{
    Q_ASSERT(!key.toString().isEmpty());
    auto gIt = lowerBoundGroup(key);
    if (gIt == m_roomGroups.end() || gIt->key != key)
    {
        const auto gPos = gIt - m_roomGroups.begin();
        const auto affectedIdxs = preparePersistentIndexChange(gPos, 1);
        if (notify)
            beginInsertRows({}, gPos, gPos);
        gIt = m_roomGroups.insert(gIt, {key, {}});
        if (notify)
        {
            endInsertRows();
            emit groupAdded(gPos);
        }
        changePersistentIndexList(affectedIdxs.first, affectedIdxs.second);
    }
    return gIt;
}

void RoomListModel::addRoomToGroups(Room* room, bool notify,
                                    QVariantList groups)
{
    if (groups.empty())
        groups = m_roomOrder->roomGroups(room);
    for (const auto& g: groups)
    {
        const auto gIt = tryInsertGroup(g, notify);
        const auto rIt = lowerBoundRoom(*gIt, room);
        if (rIt != gIt->rooms.end() && *rIt == room)
        {
            qWarning() << "RoomListModel:" << room->objectName()
                       << "is already listed under group" << g.toString();
            continue;
        }
        const auto rPos = rIt - gIt->rooms.begin();
        const auto gIdx = index(gIt - m_roomGroups.begin(), 0);
        if (notify)
            beginInsertRows(gIdx, rPos, rPos);
        gIt->rooms.insert(rIt, room);
        if (notify)
            endInsertRows();
        m_roomIndices.insert(room, index(rPos, 0, gIdx));
        qDebug() << "RoomListModel: Added" << room->objectName()
                 << "to group" << gIt->key.toString();
    }
}

void RoomListModel::connectRoomSignals(Room* room)
{
    connect(room, &Room::beforeDestruction, this, &RoomListModel::deleteRoom);
    m_roomOrder->connectSignals(room);
    connect(room, &Room::displaynameChanged,
            this, [this,room] { refresh(room); });
    connect(room, &Room::unreadMessagesChanged,
            this, [this,room] { refresh(room); });
    connect(room, &Room::notificationCountChanged,
            this, [this,room] { refresh(room); });
    connect(room, &Room::avatarChanged,
            this, [this,room] { refresh(room, { Qt::DecorationRole }); });
}

void RoomListModel::doRemoveRoom(QModelIndex idx)
{
    if (!isValidRoomIndex(idx))
    {
        qCritical() << "Attempt to remove a room at invalid index" << idx;
        Q_ASSERT(false);
        return;
    }
    const auto gPos = idx.parent().row();
    auto& group = m_roomGroups[gPos];
    const auto rIt = group.rooms.begin() + idx.row();
    qDebug() << "RoomListModel: Removing room" << (*rIt)->objectName()
             << "from group" << group.key.toString();
    if (m_roomIndices.remove(*rIt, idx) != 1)
    {
        qCritical() << "Index" << idx << "for room" << (*rIt)->objectName()
                    << "not found in the index registry";
        Q_ASSERT(false);
    }
    beginRemoveRows(idx.parent(), idx.row(), idx.row());
    group.rooms.erase(rIt);
    endRemoveRows();
    if (group.rooms.empty())
    {
        // Update persistent indices with parents after the deleted one
        const auto affectedIdxs = preparePersistentIndexChange(gPos + 1, -1);

        beginRemoveRows({}, gPos, gPos);
        m_roomGroups.remove(gPos);
        endRemoveRows();

        changePersistentIndexList(affectedIdxs.first, affectedIdxs.second);
    }
}

void RoomListModel::doRebuild()
{
    m_roomGroups.clear();
    m_roomIndices.clear();
    for (const auto& c: m_connections)
        for (auto* r: c->roomMap())
            addRoomToGroups(r, false);
}

std::pair<QModelIndexList, QModelIndexList>
RoomListModel::preparePersistentIndexChange(int fromPos, int shiftValue)
{
    QModelIndexList from, to;
    for (auto& pIdx: persistentIndexList())
        if (isValidRoomIndex(pIdx) && pIdx.parent().row() >= fromPos)
        {
            from.append(pIdx);
            to.append(createIndex(pIdx.row(), pIdx.column(),
                                  quintptr(int(pIdx.internalId()) + shiftValue)));
        }

    return { std::move(from), std::move(to) };
}

int RoomListModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
        return m_roomGroups.size();

    if (isValidGroupIndex(parent))
        return m_roomGroups[parent.row()].rooms.size();

    return 0; // Rooms have no children
}

int RoomListModel::totalRooms() const
{
    int result = 0;
    for (const auto& c: m_connections)
        result += c->roomMap().size();
    return result;
}

bool RoomListModel::isValidGroupIndex(QModelIndex i) const
{
    return i.isValid() && !i.parent().isValid() && i.row() < m_roomGroups.size();
}

bool RoomListModel::isValidRoomIndex(QModelIndex i) const
{
    return i.isValid() && isValidGroupIndex(i.parent()) &&
            i.row() < m_roomGroups[i.parent().row()].rooms.size();
}

QVariant RoomListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    if (isValidGroupIndex(index))
    {
        if (role == Qt::DisplayRole)
            return m_roomOrder->groupLabel(m_roomGroups[index.row()]);

        // It would be more proper to do it in RoomListItemDelegate
        // (see roomlistdock.cpp) but I (@kitsune) couldn't find a working way.
        if (role == Qt::BackgroundRole)
            return QGuiApplication::palette()
                   .brush(QPalette::Active, QPalette::Button);
        return {};
    }

    auto* const room = roomAt(index);
    if (!room)
        return {};
//    if (index.column() == 1)
//        return room->lastUpdated();
//    if (index.column() == 2)
//        return room->lastAttended();

    using QMatrixClient::JoinState;
    switch (role)
    {
        case Qt::DisplayRole:
        {
            const auto prefix =
                    room->isUnstable() ? QStringLiteral("(!)") : QString();
            const auto unreadCount = room->unreadCount();
            const auto postfix = unreadCount == -1 ? QString() :
                room->readMarker() != room->timelineEdge()
                    ? QStringLiteral(" [%1]").arg(unreadCount)
                    : QStringLiteral(" [%1+]").arg(unreadCount);
            for (const auto& c: m_connections)
            {
                if (c == room->connection())
                    continue;
                if (c->room(room->id(), room->joinState()))
                    return prefix + tr("%1 (as %2)", "%Room (as %user)")
                           .arg(room->displayName(), room->connection()->userId())
                           + postfix;
            }
            return prefix + room->displayName() + postfix;
        }
        case Qt::DecorationRole:
        {
            auto avatar = room->avatar(16, 16);
            if (!avatar.isNull())
                return avatar;
            switch( room->joinState() )
            {
                case JoinState::Join:
                    return QIcon(":/irc-channel-joined.svg");
                case JoinState::Invite:
                    return QIcon(":/irc-channel-invited.svg");
                case JoinState::Leave:
                    return QIcon(":/irc-channel-parted.svg");
                default:
                    Q_ASSERT(false); // Unknown JoinState?
            }
            return {}; // Shouldn't reach here
        }
        case Qt::ToolTipRole:
        {
            QString result =
                QStringLiteral("<b>%1</b>").arg(room->displayName()) % "<br>" %
                tr("Main alias: %1").arg(room->canonicalAlias()) % "<br>" %
                tr("Joined: %Ln",
                   "The number of joined members", room->joinedCount());
            if (room->invitedCount() > 0)
                result += "<br>" % tr("Invited: %Ln",
                                      "The number of invited users",
                                      room->invitedCount());

            auto directChatUsers = room->directChatUsers();
            if (!directChatUsers.isEmpty())
            {
                QStringList userNames;
                for (auto* user: directChatUsers)
                    userNames.push_back(user->displayname(room));
                result += "<br>" % tr("Direct chat with %1")
                                   .arg(userNames.join(','));
            }

            if (room->usesEncryption())
                result += "<br>" % tr("The room enforces encryption");

            if (room->isUnstable())
            {
                result += "<br>(!) " % tr("This room's version is unstable!");
                if (room->canSwitchVersions())
                    result += ' ' % tr("Consider upgrading to a stable version"
                                       " (use room settings for that)");
            }

            auto unreadCount = room->unreadCount();
            if (unreadCount >= 0)
            {
                const auto unreadLine =
                    room->readMarker() == room->timelineEdge()
                        ? tr("Unread messages: %1+")
                        : tr("Unread messages: %1");
                result += "<br>" % unreadLine.arg(unreadCount);
            }

            auto hlCount = room->highlightCount();
            if (hlCount > 0)
                result += "<br>" % tr("Unread highlights: %1").arg(hlCount);

            result += "<br>" % tr("ID: %1").arg(room->id()) % "<br>";
            auto asUser = m_connections.size() < 2 ? QString() : ' ' +
                tr("as %1",
                   "as <user account> (disambiguates entries in the room list)")
                .arg(room->localUser()->id());
            switch (room->joinState())
            {
                case JoinState::Join:
                    result += tr("You joined this room") % asUser;
                    break;
                case JoinState::Leave:
                    result += tr("You left this room") % asUser;
                    break;
                case JoinState::Invite:
                    result += tr("You were invited into this room") % asUser;
            }
            return result;
        }
        case HasUnreadRole:
            return room->hasUnreadMessages();
        case HighlightCountRole:
            return room->highlightCount();
        case JoinStateRole:
            return toCString(room->joinState()); // FIXME: better make the enum QVariant-convertible (only possible from Qt 5.8, see Q_ENUM_NS)
        case ObjectRole:
            return QVariant::fromValue(room);
        default:
            return {};
    }
}

int RoomListModel::columnCount(const QModelIndex&) const
{
    return 1;
}

void RoomListModel::updateGroups(Room* room)
{
    const auto oldRoomIndices = m_roomIndices.values(room);
    Q_ASSERT(!oldRoomIndices.empty()); // The room should have been somewhere

    auto groups = m_roomOrder->roomGroups(room);
    for (const auto& oldIndex: oldRoomIndices)
    {
        Q_ASSERT(isValidRoomIndex(oldIndex));
        const auto gIdx = oldIndex.parent();
        auto& group = m_roomGroups[gIdx.row()];
        if (groups.removeOne(group.key)) // Test and remove at once
        {
            // The room still in this group but may need to move around
            const auto oldIt = group.rooms.begin() + oldIndex.row();
            const auto newIt = lowerBoundRoom(group, room);
            if (newIt != oldIt)
            {
                beginMoveRows(gIdx, oldIndex.row(), oldIndex.row(),
                              gIdx, int(newIt - group.rooms.begin()));
                if (newIt > oldIt)
                    std::rotate(oldIt, oldIt + 1, newIt);
                else
                    std::rotate(newIt, oldIt, oldIt + 1);
                endMoveRows();
            }
            Q_ASSERT(roomAt(oldIndex) == room);
        } else
            doRemoveRoom(oldIndex); // May invalidate `group` and `gIdx`
    }
    if (!groups.empty())
        addRoomToGroups(room, true, groups); // Groups the room wasn't before
    qDebug() << "RoomListModel: groups for" << room->objectName() << "updated";
}

void RoomListModel::refresh(Room* room, const QVector<int>& roles)
{
    // The problem here is that the change might cause the room to change
    // its groups. Assume for now that such changes are processed elsewhere
    // where details about the change are available (e.g. in tagsChanged).
    visitRoom(*room,
        [this,&roles] (QModelIndex idx) { emit dataChanged(idx, idx, roles); });
}
