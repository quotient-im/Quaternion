/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2016 Felix Rohrbach <kde@fxrh.de>                        *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#include "roomlistmodel.h"

#include "../quaternionroom.h"
#include "../logging_categories.h"

#include <Quotient/eventstats.h>
#include <Quotient/user.h>
#include <Quotient/connection.h>
#include <Quotient/settings.h>

// See a comment in the same place at userlistmodel.cpp
#include <QtWidgets/QAbstractItemView>
#include <QtGui/QIcon>
#include <QtGui/QFontMetrics>
#include <QtGui/QPalette>
#include <QtGui/QBrush>
#include <QtCore/QStringBuilder>

RoomListModel::RoomListModel(QAbstractItemView* parent)
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
    connect(connection, &Connection::newRoom, this, &RoomListModel::addRoom);
    m_roomOrder->connectSignals(connection);

    const auto& allRooms = connection->allRooms();
    for (auto* r: allRooms)
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
        qCCritical(MODELS) << "RoomListModel: Invalid tag at position"
                           << index.row();
        return;
    }
    if (tag.startsWith(RoomGroup::SystemPrefix))
    {
        qCWarning(MODELS) << "RoomListModel: System groups cannot be deleted "
                             "(tried to delete"
                          << tag << "group)";
        return;
    }
    // After the below loop, the respective group will magically disappear from
    // m_roomGroups as well due to tagsChanged() triggered from removeTag()
    for (const auto& c: m_connections)
        for (auto* r: c->roomsWithTag(tag))
            r->removeTag(tag);

    Quotient::SettingsGroup("UI/RoomsDock").remove(tag);
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
            qCCritical(MODELS)
                << "Room at" << idx << "is" << roomAt(idx)->objectName()
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
    for (const auto& g: std::as_const(groups))
    {
        const auto gIt = tryInsertGroup(g);
        const auto rIt = lowerBoundRoom(*gIt, room);
        if (rIt != gIt->rooms.cend() && *rIt == room)
        {
            qCWarning(MODELS)
                << "RoomListModel:" << room->objectName()
                << "is already listed under group" << g.toString();
            continue;
        }
        const auto rPos = int(rIt - gIt->rooms.begin());
        const auto gIdx = index(int(gIt - m_roomGroups.begin()), 0);
        beginInsertRows(gIdx, rPos, rPos);
        gIt->rooms.insert(rIt, room);
        endInsertRows();
        m_roomIndices.insert(room, index(rPos, 0, gIdx));
        qCDebug(MODELS) << "RoomListModel: Added" << room->objectName()
                        << "to group" << gIt->key.toString();
    }
}

void RoomListModel::connectRoomSignals(Room* room)
{
    connect(room, &Room::beforeDestruction, this, &RoomListModel::deleteRoom);
    m_roomOrder->connectSignals(room);
    connect(room, &Room::changed, this, [this, room](Room::Changes changes) {
        using C = Room::Change;
        if ((changes
             & (C::RoomNames | C::PartiallyReadStats | C::UnreadStats
                | C::Highlights))
            > 0)
            refresh(room);
        else if (changes & C::Avatar)
            refresh(room, { Qt::DecorationRole });
    });
}

void RoomListModel::doRemoveRoom(const QModelIndex &idx)
{
    if (!isValidRoomIndex(idx))
    {
        qCCritical(MODELS) << "Attempt to remove a room at invalid index"
                           << idx;
        Q_ASSERT(false);
        return;
    }
    const auto gPos = idx.parent().row();
    auto& group = m_roomGroups[gPos]; // clazy:exclude=detaching-member
    const auto rIt =
        group.rooms.begin() + idx.row(); // clazy:exclude=detaching-member
    qCDebug(MODELS) << "RoomListModel: Removing room" << (*rIt)->objectName()
                    << "from group" << group.key.toString();
    if (m_roomIndices.remove(*rIt, idx) != 1)
    {
        qCCritical(MODELS) << "Index" << idx << "for room"
                           << (*rIt)->objectName()
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
        const auto& allRooms = c->allRooms();
        for (auto* r: allRooms) {
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

bool RoomListModel::isValidGroupIndex(const QModelIndex& i) const
{
    return i.isValid() && !i.parent().isValid() && i.row() < m_roomGroups.size();
}

bool RoomListModel::isValidRoomIndex(const QModelIndex& i) const
{
    return i.isValid() && isValidGroupIndex(i.parent()) &&
            i.row() < m_roomGroups[i.parent().row()].rooms.size();
}

inline QString toUiString(qsizetype n, const char* prefix = "",
                          const QLocale& locale = QLocale())
{
    return n ? prefix % locale.toString(n) : QString();
}

QVariant RoomListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    const auto* view = static_cast<const QAbstractItemView*>(parent());
    if (isValidGroupIndex(index))
    {
        if (role == Qt::DisplayRole) {
            int unreadRoomsCount = 0;
            for (const auto& r: m_roomGroups[index.row()].rooms)
                unreadRoomsCount += !r->unreadStats().empty();

            const auto postfix = unreadRoomsCount
                ? QStringLiteral(" [%1]").arg(unreadRoomsCount) : QString();

            return m_roomOrder->groupLabel(m_roomGroups[index.row()]).toString()
                + postfix;
        }

        // It would be more proper to do it in RoomListItemDelegate
        // (see roomlistdock.cpp) but I (@kitsune) couldn't find a working way.
        if (role == Qt::BackgroundRole)
            return view->palette().brush(QPalette::Active, QPalette::Button);

        if (role == HighlightCountRole) {
            int highlightCount = 0;
            for (auto &r: m_roomGroups[index.row()].rooms)
                highlightCount += r->highlightCount();

            return highlightCount;
        }

        return {};
    }

    auto* const room = roomAt(index);
    if (!room)
        return {};
//    if (index.column() == 1)
//        return room->lastUpdated();
//    if (index.column() == 2)
//        return room->lastAttended();

    static const auto RoomNameTemplate = tr("%1 (as %2)", "%Room (as %user)");
    auto disambiguatedName = room->displayName();
    if (role == Qt::DisplayRole || role == Qt::ToolTipRole)
        for (const auto& c: m_connections)
            if (c != room->connection()
                && c->room(room->id(), room->joinState()))
                disambiguatedName =
                    RoomNameTemplate.arg(room->displayName(),
                                         room->localMember().id());

    using Quotient::JoinState;
    switch (role)
    {
        case Qt::DisplayRole: {
            static Quotient::Settings settings;
            QString value =
                (room->isUnstable() ? "(!)" : "") % disambiguatedName;
            if (const auto& partiallyReadStats = room->partiallyReadStats();
                !partiallyReadStats.empty()) //
            {
                const auto& [countSinceFullyRead, _1, isPartiallyReadEstimate] =
                    partiallyReadStats;
                const auto& [unreadCount, highlightCount, isUnreadEstimate] =
                    room->unreadStats();
                const auto partiallyReadCount =
                    countSinceFullyRead - unreadCount;
                value +=
                    " [" % toUiString(unreadCount)
                    % (!isPartiallyReadEstimate && isUnreadEstimate ? "?" : "")
                    % (partiallyReadCount > 0
                           ? QStringLiteral("+%L1").arg(partiallyReadCount)
                           : QString())
                    % (isPartiallyReadEstimate ? "?" : "")
                    % (highlightCount > 0
                           ? QStringLiteral(", %L1").arg(highlightCount)
                           : QString())
                    % ']';
            }
            if (settings.get("Debug/read_receipts", false)
                && room->timelineSize() > 0)
            {
                const auto localReadReceipt = room->localReadReceiptMarker();
                value += " {"
                         % toUiString(room->historyEdge() - localReadReceipt,
                                      "", QLocale::C)
                         % '|'
                         % toUiString(room->syncEdge() - localReadReceipt.base(),
                                      "", QLocale::C)
                         % '}';
            }
            return value;
        }
        case Qt::DecorationRole:
        {
            const auto dpi = view->devicePixelRatioF();
            if (auto avatar = room->avatar(int(view->iconSize().height() * dpi));
                !avatar.isNull()) {
                avatar.setDevicePixelRatio(dpi);
                return QIcon(QPixmap::fromImage(avatar));
            }
            switch (room->joinState()) {
            case JoinState::Join:
                return QIcon::fromTheme("user-available",
                                        QIcon(":/irc-channel-joined"));

            case JoinState::Invite:
                return QIcon::fromTheme("contact-new",
                                        QIcon(":/irc-channel-invited"));
            case JoinState::Leave:
                return QIcon::fromTheme("user-offline",
                                        QIcon(":/irc-channel-parted"));
            default:
                Q_ASSERT(false); // Unknown JoinState?
            }
            return {}; // Shouldn't reach here
        }
        case Qt::ToolTipRole:
        {
            QString result =
                "<b>" % disambiguatedName.toHtmlEscaped() % "</b><br>"
                % tr("Main alias: %1").arg(room->canonicalAlias().toHtmlEscaped())
                % "<br>" %
                //: The number of joined members
                tr("Joined: %L1").arg(room->joinedCount());
            if (room->invitedCount() > 0)
                result += //: The number of invited users
                    "<br>" % tr("Invited: %L1").arg(room->invitedCount());

            const auto directChatMembers = room->directChatMembers();
            if (!directChatMembers.isEmpty()) {
                QStringList userNames;
                userNames.reserve(directChatMembers.size());
                for (const auto& m: directChatMembers)
                    userNames.push_back(m.htmlSafeDisplayName());
                result += "<br>"
                          % tr("Direct chat with %1").arg(QLocale().createSeparatedList(userNames));
            }

            if (room->usesEncryption())
                result += "<br>" % tr("The room enforces encryption");

            if (room->isUnstable()) {
                result += "<br>(!) " % tr("This room's version is unstable!");
                if (room->canSwitchVersions())
                    result += ' ' % tr("Consider upgrading to a stable version"
                                       " (use room settings for that)");
            }

            static const QString MaybeMore = ' ' %
                /*: Unread messages */ tr("(maybe more)");
            if (const auto s = room->partiallyReadStats(); !s.empty())
                result += "<br/>"
                          % tr("Events after fully read marker: %L1")
                                .arg(s.notableCount)
                          % (s.isEstimate ? MaybeMore : QString());

            if (const auto us = room->unreadStats(); !us.empty())
                result += "<br>"
                          % (room->highlightCount() > 0
                                 ? tr("Unread events/highlights since read "
                                      "receipt: %L1/%L2")
                                       .arg(us.notableCount)
                                       .arg(room->highlightCount())
                                 : tr("Unread events since read receipt: %L1")
                                       .arg(us.notableCount))
                          % (us.isEstimate ? MaybeMore : QString());

            // Room ids are pretty safe from rogue HTML; escape it just in case
            result += "<br>" % tr("Room id: %1").arg(room->id().toHtmlEscaped())
                      % "<br>"
                      % (room->joinState() == JoinState::Join
                             ? tr("You joined this room as %1")
                         : room->joinState() == JoinState::Invite
                             ? tr("You were invited into this room as %1")
                             : tr("You left this room as %1"))
                        .arg(room->localMember().id().toHtmlEscaped());
            return result;
        }
        case HasUnreadRole:
            return !room->unreadStats().empty();
        case HighlightCountRole:
            return room->highlightCount();
        case JoinStateRole:
            if (!room->successorId().isEmpty())
                return QStringLiteral("upgraded");
            return QVariant::fromValue(room->joinState());
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
    auto groups = m_roomOrder->roomGroups(room);

    const auto oldRoomIndices = m_roomIndices.values(room);
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
    qCDebug(MODELS) << "RoomListModel: groups for" << room->objectName()
                    << "updated";
}

void RoomListModel::refresh(Room* room, const QVector<int>& roles)
{
    // The problem here is that the change might cause the room to change
    // its groups. Assume for now that such changes are processed elsewhere
    // where details about the change are available (e.g. in tagsChanged).
    visitRoom(*room, [this,&roles] (const QModelIndex &idx) {
        emit dataChanged(idx, idx, roles);
        emit dataChanged(idx.parent(), idx.parent(), roles);
    });
}
