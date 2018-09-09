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

#include <functional>

using namespace std::placeholders;

const RoomGroups& AbstractRoomOrdering::modelGroups() const
{
    return model()->m_roomGroups;
}

RoomGroups& AbstractRoomOrdering::modelGroups()
{
    return model()->m_roomGroups;
}

AbstractRoomOrdering::slot_closure_t
AbstractRoomOrdering::getPrepareToUpdateGroupsSlot(Room* room)
{
    return std::bind(&RoomListModel::prepareToUpdateGroups, model(), room);
}

AbstractRoomOrdering::slot_closure_t
AbstractRoomOrdering::getUpdateGroupsSlot(AbstractRoomOrdering::Room* room)
{
    return std::bind(&RoomListModel::updateGroups, model(), room);
}

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

void AbstractRoomOrdering::addRoomToGroups(QMatrixClient::Room* room,
                                           const QVariantList& groups)
{
    model()->addRoomToGroups(room, true, groups);
}

void AbstractRoomOrdering::removeRoomFromGroup(QMatrixClient::Room* room,
                                               const QVariant& groupCaption)
{
    model()->doRemoveRoom(model()->indexOf(groupCaption, room));
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

const QString OrderByTag::DirectChat = QStringLiteral("org.qmatrixclient.direct");
const QString OrderByTag::Untagged = QStringLiteral("org.qmatrixclient.none");

QVariant OrderByTag::groupLabel(const RoomGroup& g) const
{
    static const auto FavouritesLabel = RoomListModel::tr("Favourites");
    static const auto LowPriorityLabel = RoomListModel::tr("Low priority");
    static const auto DirectChatsLabel = RoomListModel::tr("People");
    static const auto UngroupedRoomsLabel = RoomListModel::tr("Ungrouped rooms");

    const auto caption =
            g.key == Untagged ? UngroupedRoomsLabel :
            g.key == DirectChat ? DirectChatsLabel :
            g.key == QMatrixClient::FavouriteTag ? FavouritesLabel :
            g.key == QMatrixClient::LowPriorityTag ? LowPriorityLabel :
            g.key.toString().startsWith("u.") ? g.key.toString().mid(2) :
            g.key.toString();
    return RoomListModel::tr("%1 (%Ln room(s))", "", g.rooms.size()).arg(caption);
}

bool OrderByTag::groupLessThan(const RoomGroup& g1, const QVariant& g2key) const
{
    static auto tagsOrder = initTagsOrder();
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
        return false; // Short-circuit

    const auto& tag = groupKey.toString();
    // First, try to compare tag orders; if neither of them is less
    // than the other, check if it's two incarnations of the same
    // room with different states; if not, just order by id.
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

    if (r1->id() == r2->id())
        return r1->joinState() < r2->joinState();

    {
        auto dbg = qDebug();
        dbg << "RoomListModel: couldn't strongly order"
            << r1->objectName() << "and" << r2->objectName()
            << "under" << tag << "tag: both have order value";
        if (o1.omitted())
            dbg << "omitted";
        else
            dbg << o1.value();
    }
    return r1->id() < r2->id(); // FIXME: switch to displayName() when the library gets Room::displaynameAboutToChange()
}

AbstractRoomOrdering::groups_t OrderByTag::roomGroups(const Room* room) const
{
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
    using JoinState = QMatrixClient::JoinState;
    QObject::connect( connection, &Connection::directChatsListChanged, model(),
        [this,connection] (DCMap additions, DCMap removals) {
            for (const auto& rId: additions)
                for (auto r: {connection->room(rId, JoinState::Invite),
                              connection->room(rId, JoinState::Join)})
                    if (r)
                    {
                        if (r->tags().empty())
                            removeRoomFromGroup(r, Untagged);
                        addRoomToGroups(r, {{ DirectChat }});
                    }

            for (const auto& rId: removals)
                for (auto r: {connection->room(rId, JoinState::Invite),
                              connection->room(rId, JoinState::Join)})
                    if (r)
                    {
                        removeRoomFromGroup(r, DirectChat);
                        if (r->tags().empty())
                            addRoomToGroups(r, {{ Untagged }});
                    }
        });
}

void OrderByTag::connectSignals(Room* room)
{
    QObject::connect(room, &QMatrixClient::Room::tagsAboutToChange,
                     model(), getPrepareToUpdateGroupsSlot(room));
    QObject::connect(room, &QuaternionRoom::tagsChanged,
                     model(), getUpdateGroupsSlot(room));
}

QStringList OrderByTag::initTagsOrder()
{
    static const QStringList DefaultTagsOrder {
        QMatrixClient::FavouriteTag, QStringLiteral("u.*"), DirectChat, Untagged,
        QMatrixClient::LowPriorityTag
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
{ }

void RoomListModel::addConnection(QMatrixClient::Connection* connection)
{
    Q_ASSERT(connection);

    using namespace QMatrixClient;
    beginResetModel();
    m_connections.emplace_back(connection, this);
    connect( connection, &Connection::loggedOut,
             this, [=]{ deleteConnection(connection); } );
    connect( connection, &Connection::invitedRoom,
             this, &RoomListModel::addOrUpdateRoom);
    connect( connection, &Connection::joinedRoom,
             this, &RoomListModel::addOrUpdateRoom);
    connect( connection, &Connection::leftRoom,
             this, &RoomListModel::addOrUpdateRoom);
    m_roomOrder->connectSignals(connection);

    for( auto r: connection->roomMap() )
    {
        addRoomToGroups(r, false);
        connectRoomSignals(r);
    }
    endResetModel();
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

    beginResetModel();
    for (auto& group: m_roomGroups)
        group.rooms.erase(
            std::remove_if(group.rooms.begin(), group.rooms.end(),
                [connection](const auto* room) {
                    return room->connection() == connection;
                }), group.rooms.end());
    m_roomGroups.erase(
        std::remove_if(m_roomGroups.begin(), m_roomGroups.end(),
            [=](const RoomGroup& rg) { return rg.rooms.empty(); }),
        m_roomGroups.end());
    m_connections.erase(connIt);
    connection->disconnect(this);
    endResetModel();
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

int RoomListModel::getRoomGroupOffset(QModelIndex index) const
{
    Q_ASSERT(index.isValid()); // Root item shouldn't come here
    // If we're on a room, find its group; otherwise just take the index
    return (index.parent().isValid() ? index.parent() : index).row();
}

void RoomListModel::visitRoom(QMatrixClient::Room* room,
                              const std::function<void(QModelIndex)>& visitor)
{
    Q_ASSERT(room);
    for (const auto& g: m_roomOrder->roomGroups(room))
    {
        const auto idx = indexOf(g, room);
        if (!isValidGroupIndex(idx.parent()))
        {
            qWarning() << "RoomListModel: Invalid group index for group"
                       << g.toString() << "with room" << room->objectName();
            Q_ASSERT(false);
            continue;
        }
        if (!isValidRoomIndex(idx))
        {
            qCritical() << "RoomListModel: the current order lists room"
                        << room->objectName() << "in group" << g.toString()
                        << "but the model doesn't have it";
            Q_ASSERT(false);
            continue;
        }
        Q_ASSERT(roomAt(idx) == room);
        visitor(idx);
    }
}

QVariant RoomListModel::roomGroupAt(QModelIndex idx) const
{
    const auto groupIt = m_roomGroups.cbegin() + getRoomGroupOffset(idx);
    return groupIt != m_roomGroups.end() ? groupIt->key : QVariant();
}

QuaternionRoom* RoomListModel::roomAt(QModelIndex idx) const
{
    return isValidRoomIndex(idx) ?
            static_cast<QuaternionRoom*>(
                  m_roomGroups[idx.parent().row()].rooms[idx.row()]) : nullptr;
}

QModelIndex RoomListModel::indexOf(const QVariant& group,
                                   QMatrixClient::Room* room) const
{
    const auto groupIt = lowerBoundGroup(group);
    if (groupIt == m_roomGroups.end() || groupIt->key != group)
        return {}; // Group not found
    const auto groupIdx = index(groupIt - m_roomGroups.begin(), 0);
    if (!room)
        return groupIdx; // Group caption

    const auto rIt = lowerBoundRoom(*groupIt, room);
    if (rIt == groupIt->rooms.end() || *rIt != room)
        return {}; // Room not found in this group

    return index(rIt - groupIt->rooms.begin(), 0, groupIdx);
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

void RoomListModel::addOrUpdateRoom(QMatrixClient::Room* room,
                                    QMatrixClient::Room* prev)
{
    // There are two cases when this method is called:
    // 1. (prev == nullptr) adding a new room to the room list
    // 2. (prev != nullptr) accepting/rejecting an invitation or inviting to
    //    the previously left room (in both cases prev has the previous state).
    if (prev == room)
    {
        qCritical() << "RoomListModel::updateRoom: room tried to replace itself";
        refresh(room);
        return;
    }

    if (prev)
    {
        if (room->id() != prev->id())
        {
            qCritical() << "RoomListModel::updateRoom: attempt to update room"
                        << prev->id() << "to" << room->id();
            // That doesn't look right but technically we still can do it.
        }
        if (m_roomOrder->roomGroups(prev) ==
                m_roomOrder->roomGroups(room))
        {
            qDebug() << "RoomListModel: replacing room" << prev->objectName()
                     << "in" << toCString(prev->joinState()) << "state with"
                     << toCString(room->joinState());
            visitRoom(prev, [this,room] (QModelIndex idx) {
                m_roomGroups[idx.parent().row()].rooms[idx.row()] = room;
                emit dataChanged(idx, idx);
            });
            prev->disconnect(this);
            return;
        }
        deleteRoom(prev);
    }
    addRoomToGroups(room);
    connectRoomSignals(room);
}

void RoomListModel::deleteRoom(QMatrixClient::Room* room)
{
    visitRoom(room, std::bind(&RoomListModel::doRemoveRoom, this, _1));
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
        if (notify)
            beginInsertRows({}, gPos, gPos);
        gIt = m_roomGroups.insert(gIt, {key, {}});
        if (notify)
        {
            endInsertRows();
            emit groupAdded(gPos);
        }
    }
    return gIt;
}

void RoomListModel::addRoomToGroups(QMatrixClient::Room* room, bool notify,
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
        if (notify)
            beginInsertRows(index(gIt - m_roomGroups.begin(), 0), rPos, rPos);
        gIt->rooms.insert(rIt, room);
        if (notify)
            endInsertRows();
        qDebug() << "RoomListModel: Added" << room->objectName()
                 << "to group" << gIt->key.toString();
    }
}

void RoomListModel::connectRoomSignals(QMatrixClient::Room* room)
{
    using QMatrixClient::Room;
    connect(room, &Room::displaynameChanged,
            this, &RoomListModel::displaynameChanged);
    connect(room, &Room::unreadMessagesChanged,
            this, &RoomListModel::unreadMessagesChanged);
    connect(room, &Room::notificationCountChanged,
            this, &RoomListModel::unreadMessagesChanged);
    connect(room, &Room::joinStateChanged,
            this, [this,room] { refresh(room); });
    connect(room, &Room::avatarChanged,
            this, [this,room] { refresh(room, { Qt::DecorationRole }); });
    connect(room, &Room::beforeDestruction, this, &RoomListModel::deleteRoom);
    m_roomOrder->connectSignals(room);
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
    beginRemoveRows(idx.parent(), idx.row(), idx.row());
    group.rooms.erase(rIt);
    endRemoveRows();
    if (group.rooms.empty())
    {
        beginRemoveRows({}, gPos, gPos);
        m_roomGroups.remove(gPos);
        endRemoveRows();
    }
}

void RoomListModel::doRebuild()
{
    m_roomGroups.clear();
    for (const auto& c: m_connections)
        for (auto* r: c->roomMap())
            addRoomToGroups(r, false);
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
                    return tr("%1 (as %2)").arg(room->displayName(),
                                                room->connection()->userId())
                           + postfix;
            }
            return room->displayName() + postfix;
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
            }
        }
        case Qt::ToolTipRole:
        {
            auto result = QStringLiteral("<b>%1</b><br>").arg(room->displayName());
            result += tr("Main alias: %1<br>").arg(room->canonicalAlias());
            result += tr("Members: %1<br>").arg(room->memberCount());

            auto directChatUsers = room->directChatUsers();
            if (!directChatUsers.isEmpty())
            {
                QStringList userNames;
                for (auto* user: directChatUsers)
                    userNames.push_back(user->displayname(room));
                result += tr("Direct chat with %1<br>")
                            .arg(userNames.join(','));
            }

            if (room->usesEncryption())
                result += tr("The room enforces encryption<br>");

            auto unreadCount = room->unreadCount();
            if (unreadCount >= 0)
            {
                const auto unreadLine =
                    room->readMarker() == room->timelineEdge()
                        ? tr("Unread messages: %1+<br>")
                        : tr("Unread messages: %1<br>");
                result += unreadLine.arg(unreadCount);
            }

            auto hlCount = room->highlightCount();
            if (hlCount > 0)
                result += tr("Unread highlights: %1<br>").arg(hlCount);

            result += tr("ID: %1<br>").arg(room->id());
            switch (room->joinState())
            {
                case JoinState::Join:
                    result += tr("You joined this room");
                    break;
                case JoinState::Leave:
                    result += tr("You left this room");
                    break;
                case JoinState::Invite:
                    result += tr("You were invited into this room");
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

void RoomListModel::displaynameChanged(QMatrixClient::Room* room)
{
    refresh(room);
}

void RoomListModel::unreadMessagesChanged(QMatrixClient::Room* room)
{
    refresh(room);
}

void RoomListModel::prepareToUpdateGroups(QMatrixClient::Room* room)
{
    Q_ASSERT(m_roomIdxCache.empty()); // Not in the midst of another update

    const auto& groups = m_roomOrder->roomGroups(room);
    for (const auto& g: groups)
    {
        const auto& rIdx = indexOf(g, room);
        Q_ASSERT(isValidRoomIndex(rIdx));
        m_roomIdxCache.push_back(rIdx);
    }
}

void RoomListModel::updateGroups(QMatrixClient::Room* room)
{
    auto groups = m_roomOrder->roomGroups(room);
    for (const auto& oldIndex: qAsConst(m_roomIdxCache))
    {
        Q_ASSERT(isValidRoomIndex(oldIndex));
        const auto gIdx = oldIndex.parent();
        auto& group = m_roomGroups[gIdx.row()];
        if (groups.removeOne(group.key)) // Test and remove at once
        {
            const auto oldIt = group.rooms.begin() + oldIndex.row();
            const auto newIt = lowerBoundRoom(group, room);
            if (newIt == oldIt)
                continue;

            beginMoveRows(gIdx, oldIndex.row(), oldIndex.row(),
                          gIdx, int(newIt - group.rooms.begin()));
            std::move(oldIt, oldIt + 1, newIt);
            endMoveRows();
        } else
            doRemoveRoom(oldIndex); // May invalidate `group`
    }
    m_roomIdxCache.clear();
    addRoomToGroups(room, true, groups); // Groups the room wasn't before
}

void RoomListModel::refresh(QMatrixClient::Room* room,
                            const QVector<int>& roles)
{
    // The problem here is that the change might cause the room to change
    // its groups. Assume for now that such changes are processed elsewhere
    // where details about the change are available (e.g. in tagsChanged).
    visitRoom(room,
        [this,&roles] (QModelIndex idx) { emit dataChanged(idx, idx, roles); });
}
