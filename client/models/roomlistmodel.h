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

        QVariant roomGroupAt(QModelIndex index) const;
        QuaternionRoom* roomAt(QModelIndex index) const;
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

    private slots:
        void displaynameChanged(QuaternionRoom* room);
        void unreadMessagesChanged(QuaternionRoom* room);
        void tagsChanged(QuaternionRoom* room,
                         QMatrixClient::TagsMap additions, QStringList removals);
        void refresh(QuaternionRoom* room, const QVector<int>& roles = {});

        void replaceRoom(QMatrixClient::Room* room,
                         QMatrixClient::Room* prev);
        void deleteRoom(QMatrixClient::Room* room);

    private:
        std::vector<ConnectionsGuard<QMatrixClient::Connection>> m_connections;

        struct RoomGroup
        {
            QVariant caption;
            QVector<QuaternionRoom*> rooms;

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
            /// Merge rooms from another group
            void mergeWith(RoomGroup&& g);
        };
        QVector<RoomGroup> m_roomGroups;

        struct RoomOrder
        {
            Grouping grouping;
            Sorting sorting;

            std::function<bool(const RoomGroup&, const QVariant&)> groupLessThan;
            using room_lessthan_t = std::function<bool(const QuaternionRoom*,
                                                       const QuaternionRoom*)>;
            std::function<room_lessthan_t(const QVariant&)> roomLessThanFactory;
            std::function<QVariantList(const QuaternionRoom*)> groups;
        };
        RoomOrder m_roomOrder;

        // Beware, these iterators are as short-lived as QModelIndex'es
        using group_iter_t = decltype(m_roomGroups)::iterator;
        using group_citer_t = decltype(m_roomGroups)::const_iterator;
        using room_iter_t = decltype(RoomGroup::rooms)::iterator;
        using room_citer_t = decltype(RoomGroup::rooms)::const_iterator;
        using room_locator_t = std::pair<group_iter_t, room_iter_t>;
        using room_clocator_t = std::pair<group_citer_t, room_citer_t>;

        group_iter_t tryInsertGroup(const QVariant& group, bool notify = false);
        void insertRoom(QMatrixClient::Room* r, bool notify = false);
        void connectRoomSignals(QuaternionRoom* room);
        room_locator_t doRemoveRoom(group_iter_t gIt, room_iter_t rIt);

        int getRoomGroupOffset(QModelIndex index) const;
        group_iter_t getRoomGroupFor(QModelIndex index);
        group_citer_t getRoomGroupFor(QModelIndex index) const;
        int getRoomOffset(QModelIndex index, const RoomGroup& group) const;
        room_locator_t getRoomFor(QModelIndex index);
        room_clocator_t getRoomFor(QModelIndex index) const;

        group_iter_t lowerBoundGroup(const QVariant& group);
        group_citer_t lowerBoundGroup(const QVariant& group) const;
        room_iter_t lowerBoundRoom(RoomGroup& group, QuaternionRoom* room);
        room_citer_t lowerBoundRoom(const RoomGroup& group,
                                    QuaternionRoom* room) const;
        void visitRoom(QuaternionRoom* room,
                       const std::function<void(group_iter_t, room_iter_t)>& visitor);

        void doRebuild();
};
