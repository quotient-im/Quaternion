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

#include "../quaternionroom.h"
#include <connection.h>
#include <util.h>

#include <QtCore/QAbstractItemModel>

class RoomListModel: public QAbstractItemModel
{
        Q_OBJECT
        template <typename T>
        using ConnectionsGuard = QMatrixClient::ConnectionsGuard<T>;
    public:
        enum Roles {
            HasUnreadRole = Qt::UserRole + 1,
            HighlightCountRole, JoinStateRole, ObjectRole
        };

        enum Grouping {
            GroupByTag
        };

        enum Sorting {
            SortByName
        };

        explicit RoomListModel(QObject* parent = nullptr);

        void addConnection(QMatrixClient::Connection* connection);
        void deleteConnection(QMatrixClient::Connection* connection);
        void deleteTag(QModelIndex index);

        QVariant roomGroupAt(QModelIndex idx) const;
        QuaternionRoom* roomAt(QModelIndex idx) const;
        QModelIndex indexOf(const QVariant& group,
                            QuaternionRoom* room = nullptr) const;

        QModelIndex index(int row, int column,
                          const QModelIndex& parent = {}) const override;
        QModelIndex parent(const QModelIndex& index) const override;
        QVariant data(const QModelIndex& index, int role) const override;
        int columnCount(const QModelIndex&) const override;
        int rowCount(const QModelIndex& parent) const override;
        int totalRooms() const;
        bool isValidGroupIndex(QModelIndex i) const;
        bool isValidRoomIndex(QModelIndex i) const;

        void setOrder(Grouping grouping, Sorting sorting);

    signals:
        void groupAdded(int row);

    private slots:
        void displaynameChanged(QuaternionRoom* room);
        void unreadMessagesChanged(QuaternionRoom* room);
        void prepareToUpdateGroups(QuaternionRoom* room);
        void updateGroups(QuaternionRoom* room);
        void refresh(QuaternionRoom* room, const QVector<int>& roles = {});

        void replaceRoom(QMatrixClient::Room* room,
                         QMatrixClient::Room* prev);
        void deleteRoom(QMatrixClient::Room* room);

    private:
        std::vector<ConnectionsGuard<QMatrixClient::Connection>> m_connections;

        struct RoomGroup
        {
            QVariant caption;
            using rooms_t = QVector<QuaternionRoom*>;
            rooms_t rooms;

            bool operator==(const RoomGroup& other) const
            {
                return caption == other.caption;
            }
            bool operator!=(const RoomGroup& other) const
            {
                return !(*this == other);
            }
            bool operator==(const QVariant& otherCaption) const
            {
                return caption == otherCaption;
            }
            bool operator!=(const QVariant& otherCaption) const
            {
                return !(*this == otherCaption);
            }
            friend bool operator==(const QVariant& otherCaption,
                                   const RoomGroup& group)
            {
                return group == otherCaption;
            }
            friend bool operator!=(const QVariant& otherCaption,
                                   const RoomGroup& group)
            {
                return !(group == otherCaption);
            }
        };
        using groups_t = QVector<RoomGroup>;

        groups_t m_roomGroups;
        
        // Beware, these iterators are as short-lived as QModelIndex'es
        using group_iter_t = groups_t::iterator;
        using group_citer_t = groups_t::const_iterator;
        using room_iter_t = RoomGroup::rooms_t::iterator;
        using room_citer_t = RoomGroup::rooms_t::const_iterator;

        QVector<QPersistentModelIndex> m_roomIdxCache;

        struct RoomOrder
        {
            Grouping grouping;
            Sorting sorting;

            std::function<bool(const RoomGroup&, const QVariant&)> groupLessThan;
            using room_lessthan_t = std::function<bool(const QuaternionRoom*,
                                                       const QuaternionRoom*)>;
            std::function<room_lessthan_t(const QVariant&)> roomLessThanFactory;
            using groups_t = QVariantList;
            std::function<groups_t(const QuaternionRoom*)> groups;
            std::function<void(QuaternionRoom*)> connectRoomSignals;
        };
        RoomOrder m_roomOrder;

        group_iter_t tryInsertGroup(const QVariant& group, bool notify = false);
        void insertRoomToGroups(const QVariantList& groups, QuaternionRoom* room,
                                bool notify = false);
        void insertRoom(QMatrixClient::Room* r, bool notify = false);
        void connectRoomSignals(QuaternionRoom* room);
        void doRemoveRoom(QModelIndex idx);

        int getRoomGroupOffset(QModelIndex index) const;
        group_iter_t getRoomGroupFor(QModelIndex index);
        group_citer_t getRoomGroupFor(QModelIndex index) const;

        group_iter_t lowerBoundGroup(const QVariant& group);
        group_citer_t lowerBoundGroup(const QVariant& group) const;
        room_iter_t lowerBoundRoom(RoomGroup& group, QuaternionRoom* room);
        room_citer_t lowerBoundRoom(const RoomGroup& group,
                                    QuaternionRoom* room) const;
        void visitRoom(QuaternionRoom* room,
                       const std::function<void(QModelIndex)>& visitor);

        void doRebuild();
};
