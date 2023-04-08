/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2016 Felix Rohrbach <kde@fxrh.de>                        *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#pragma once

#include "abstractroomordering.h"
#include "../quaternionroom.h"

#include <connection.h>
#include <util.h>
#include <qt_connection_util.h>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QMultiHash>

class QAbstractItemView;

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

        explicit RoomListModel(QAbstractItemView* parent);
        ~RoomListModel() override = default;

        QVariant roomGroupAt(QModelIndex idx) const;
        QuaternionRoom* roomAt(QModelIndex idx) const;
        QModelIndex indexOf(const QVariant& group) const;
        QModelIndex indexOf(const QVariant& group, Room* room) const;

        QModelIndex index(int row, int column,
                          const QModelIndex& parent = {}) const override;
        QModelIndex parent(const QModelIndex& index) const override;
        using QObject::parent;
        QVariant data(const QModelIndex& index, int role) const override;
        int columnCount(const QModelIndex&) const override;
        int rowCount(const QModelIndex& parent) const override;
        int totalRooms() const;
        bool isValidGroupIndex(const QModelIndex& i) const;
        bool isValidRoomIndex(const QModelIndex& i) const;

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
        void doRemoveRoom(const QModelIndex& idx);

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
