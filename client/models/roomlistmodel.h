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
#include <QtCore/QMultiHash>

struct RoomGroup
{
    QVariant key;
    QVector<QMatrixClient::Room*> rooms;

    bool operator==(const RoomGroup& other) const
    {
        return key == other.key;
    }
    bool operator!=(const RoomGroup& other) const
    {
        return !(*this == other);
    }
    bool operator==(const QVariant& otherCaption) const
    {
        return key == otherCaption;
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
using RoomGroups = QVector<RoomGroup>;

class RoomListModel;

class AbstractRoomOrdering
{
    public:
        using Room = QMatrixClient::Room;
        using Connection = QMatrixClient::Connection;
        using groups_t = QVariantList;

        AbstractRoomOrdering(RoomListModel* m) : _model(m) { }
        virtual ~AbstractRoomOrdering() = default;

    public: // Overridables
        virtual QString orderingName() const = 0;
        virtual QVariant groupLabel(const RoomGroup& g) const = 0;
        virtual bool groupLessThan(const RoomGroup& g1,
                                   const QVariant& g2key) const = 0;
        virtual bool roomLessThan(const QVariant& group,
                                  const Room* r1, const Room* r2) const = 0;

        virtual groups_t roomGroups(const Room* room) const = 0;
        virtual void connectSignals(Connection* connection) = 0;
        virtual void connectSignals(Room* room) = 0;

    public:
        using groupLessThan_closure_t =
                std::function<bool(const RoomGroup&, const QVariant&)>;
        groupLessThan_closure_t groupLessThanFactory() const;

        using roomLessThan_closure_t =
                std::function<bool(const Room*, const Room*)>;
        roomLessThan_closure_t roomLessThanFactory(const QVariant& group) const;

    protected:
        const RoomListModel* model() const { return _model; }
        RoomListModel* model() { return _model; }
//        const RoomGroups& modelGroups() const;
//        RoomGroups& modelGroups();

        void updateGroups(Room* room);

    private:
        RoomListModel* _model;
};

class OrderByTag : public AbstractRoomOrdering
{
    public:
        explicit OrderByTag(RoomListModel* m)
            : AbstractRoomOrdering(m), tagsOrder(initTagsOrder())
        { }

        QString orderingName() const override { return QStringLiteral("tag"); }
        QVariant groupLabel(const RoomGroup& g) const override;
        bool groupLessThan(const RoomGroup& g1,
                           const QVariant& g2key) const override;
        bool roomLessThan(const QVariant& groupKey,
                          const Room* r1, const Room* r2) const override;

        groups_t roomGroups(const Room* room) const override;
        void connectSignals(Connection* connection) override;
        void connectSignals(Room* room) override;

    private:
        QStringList tagsOrder;

        static const QString Invite;
        static const QString DirectChat;
        static const QString Untagged;
        static const QString Left;

        static QStringList initTagsOrder();
};

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

        using Room = QMatrixClient::Room;

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
        void addConnection(QMatrixClient::Connection* connection);
        void deleteConnection(QMatrixClient::Connection* connection);

        // FIXME, QMatrixClient/libqmatrixclient#63:
        // This should go to the library's ConnectionManager/RoomManager
        void deleteTag(QModelIndex index);

    private slots:
        void addRoom(Room* room);
        void refresh(Room* room, const QVector<int>& roles = {});
        void deleteRoom(Room* room);

        void updateGroups(Room* room);

    private:
        friend class AbstractRoomOrdering;

        std::vector<ConnectionsGuard<QMatrixClient::Connection>> m_connections;
        RoomGroups m_roomGroups;
        std::unique_ptr<AbstractRoomOrdering> m_roomOrder;

        QMultiHash<const Room*, QPersistentModelIndex> m_roomIndices;

        RoomGroups::iterator tryInsertGroup(const QVariant& key);
        void addRoomToGroups(Room* room, QVariantList groups = {});
        void connectRoomSignals(Room* room);
        void doRemoveRoom(QModelIndex idx);

        void visitRoom(const Room& room,
                       const std::function<void(QModelIndex)>& visitor);

        void doSetOrder(std::unique_ptr<AbstractRoomOrdering>&& newOrder);

        std::pair<QModelIndexList, QModelIndexList>
        preparePersistentIndexChange(int fromPos, int shiftValue);

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
