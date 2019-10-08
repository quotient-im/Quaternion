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

RoomListModel::RoomListModel(QObject* parent)
    : QAbstractItemModel(parent)
{
    connect(this, &RoomListModel::modelAboutToBeReset,
            this, &RoomListModel::saveCurrentSelection);
    connect(this, &RoomListModel::modelReset,
            this, &RoomListModel::restoreCurrentSelection);
}

void RoomListModel::addConnection(Quotient::Connection* connection)
{
    Q_ASSERT(connection);

    using namespace Quotient;
    m_connections.emplace_back(connection, this);
    connect( connection, &Connection::loggedOut,
             this, [=]{ deleteConnection(connection); } );
    connect( connection, &Connection::newRoom,
             this, &RoomListModel::addRoom);
    m_roomOrder->connectSignals(connection);

    for (auto* r: connection->allRooms())
        addRoom(r);
}

void RoomListModel::deleteConnection(Quotient::Connection* connection)
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

    for (auto* r: connection->allRooms())
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
    if (tag.startsWith(RoomGroup::SystemPrefix))
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
    Q_ASSERT(room && !room->id().isEmpty());
    addRoomToGroups(room);
    connectRoomSignals(room);
}

void RoomListModel::deleteRoom(Room* room)
{
    visitRoom(*room, [this] (QModelIndex idx) { doRemoveRoom(idx); });
    room->disconnect(this);
}

RoomGroups::iterator RoomListModel::tryInsertGroup(const QVariant& key)
{
    Q_ASSERT(!key.toString().isEmpty());
    auto gIt = lowerBoundGroup(key);
    if (gIt == m_roomGroups.end() || gIt->key != key)
    {
        const auto gPos = gIt - m_roomGroups.begin();
        const auto affectedIdxs = preparePersistentIndexChange(gPos, 1);
        beginInsertRows({}, gPos, gPos);
        gIt = m_roomGroups.insert(gIt, {key, {}});
        endInsertRows();
        changePersistentIndexList(affectedIdxs.first, affectedIdxs.second);
        emit groupAdded(gPos);
    }
    // Check that the group is healthy
    Q_ASSERT(gIt->key == key
             && (gIt->rooms.empty() || !gIt->rooms.front()->id().isEmpty()));
    return gIt;
}

void RoomListModel::addRoomToGroups(Room* room, QVariantList groups)
{
    if (groups.empty())
        groups = m_roomOrder->roomGroups(room);
    for (const auto& g: groups)
    {
        const auto gIt = tryInsertGroup(g);
        const auto rIt = lowerBoundRoom(*gIt, room);
        if (rIt != gIt->rooms.end() && *rIt == room)
        {
            qWarning() << "RoomListModel:" << room->objectName()
                       << "is already listed under group" << g.toString();
            continue;
        }
        const auto rPos = rIt - gIt->rooms.begin();
        const auto gIdx = index(gIt - m_roomGroups.begin(), 0);
        beginInsertRows(gIdx, rPos, rPos);
        gIt->rooms.insert(rIt, room);
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

void RoomListModel::doSetOrder(std::unique_ptr<AbstractRoomOrdering>&& newOrder)
{
    beginResetModel();
    m_roomGroups.clear();
    m_roomIndices.clear();
    if (m_roomOrder)
        m_roomOrder->deleteLater();
    m_roomOrder = newOrder.release();
    endResetModel();
    for (const auto& c: m_connections)
    {
        m_roomOrder->connectSignals(c);
        for (auto* r: c->allRooms())
        {
            addRoomToGroups(r);
            m_roomOrder->connectSignals(r);
        }
    }
}

std::pair<QModelIndexList, QModelIndexList>
RoomListModel::preparePersistentIndexChange(int fromPos, int shiftValue) const
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
        result += c->allRooms().size();
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

    using Quotient::JoinState;
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

            auto nfCount = room->notificationCount();
            if (nfCount > 0)
                result += "<br>" % tr("Unread notifications: %1").arg(nfCount);

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
            if (!room->successorId().isEmpty())
                return QStringLiteral("upgraded");
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
        addRoomToGroups(room, groups); // Groups the room wasn't before
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
