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

#pragma once

#include "abstractroomordering.h"
#include "../quaternionroom.h"
#include <connection.h>
#include <util.h>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QMultiHash>

class RoomListModel: public QAbstractItemModel
{
        Q_OBJECT
        template <typename T>
        using ConnectionsGuard = Quotient::ConnectionsGuard<T>;
    public:
        enum Roles {
            HasUnreadRole = Qt::UserRole + 1,
            HighlightCountRole, JoinStateRole, ObjectRole
        };

        using Room = Quotient::Room;

        explicit RoomListModel(QObject* parent = nullptr);
        ~RoomListModel() override = default;

        QVariant roomGroupAt(QModelIndex idx) const;
        QuaternionRoom* roomAt(QModelIndex idx) const;
        QModelIndex indexOf(const QVariant& group) const;
        QModelIndex indexOf(const QVariant& group, Room* room) const;

        QModelIndex index(int row, int column,
                          const QModelIndex& parent = {}) const override;
        QModelIndex parent(const QModelIndex& index) const override;
        QVariant data(const QModelIndex& index, int role) const override;
        int columnCount(const QModelIndex&) const override;
        int rowCount(const QModelIndex& parent) const override;
        int totalRooms() const;
        bool isValidGroupIndex(QModelIndex i) const;
        bool isValidRoomIndex(QModelIndex i) const;

        template <typename OrderT>
        void setOrder() { doSetOrder(std::make_unique<OrderT>(this)); }

    signals:
        void groupAdded(int row);
        void saveCurrentSelection();
        void restoreCurrentSelection();

    public slots:
        void addConnection(Quotient::Connection* connection);
        void deleteConnection(Quotient::Connection* connection);

        // FIXME, quotient-im/libQuotient#63:
        // This should go to the library's ConnectionManager/RoomManager
        void deleteTag(QModelIndex index);

    private slots:
        void addRoom(Room* room);
        void refresh(Room* room, const QVector<int>& roles = {});
        void deleteRoom(Room* room);

        void updateGroups(Room* room);

    private:
        friend class AbstractRoomOrdering;

        std::vector<ConnectionsGuard<Quotient::Connection>> m_connections;
        RoomGroups m_roomGroups;
        AbstractRoomOrdering* m_roomOrder = nullptr;

        QMultiHash<const Room*, QPersistentModelIndex> m_roomIndices;

        RoomGroups::iterator tryInsertGroup(const QVariant& key);
        void addRoomToGroups(Room* room, QVariantList groups = {});
        void connectRoomSignals(Room* room);
        void doRemoveRoom(QModelIndex idx);

        void visitRoom(const Room& room,
                       const std::function<void(QModelIndex)>& visitor);

        void doSetOrder(std::unique_ptr<AbstractRoomOrdering>&& newOrder);

        std::pair<QModelIndexList, QModelIndexList>
        preparePersistentIndexChange(int fromPos, int shiftValue) const;

        // Beware, the returned iterators are as short-lived as QModelIndex'es
        auto lowerBoundGroup(const QVariant& group)
        {
            return std::lower_bound(m_roomGroups.begin(), m_roomGroups.end(),
                                    group, m_roomOrder->groupLessThanFactory());
        }
        auto lowerBoundGroup(const QVariant& group) const
        {
            return std::lower_bound(m_roomGroups.begin(), m_roomGroups.end(),
                                    group, m_roomOrder->groupLessThanFactory());
        }

        auto lowerBoundRoom(RoomGroup& group, Room* room) const
        {
            return std::lower_bound(group.rooms.begin(), group.rooms.end(),
                    room, m_roomOrder->roomLessThanFactory(group.key));
        }

        auto lowerBoundRoom(const RoomGroup& group, Room* room) const
        {
            return std::lower_bound(group.rooms.begin(), group.rooms.end(),
                    room, m_roomOrder->roomLessThanFactory(group.key));
        }
};
