/******************************************************************************
 * Copyright (C) 2018-2019 QMatrixClient Project
 *
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
 */

#pragma once

#include <room.h>
#include <QtCore/QVariantList>

struct RoomGroup
{
    QVariant key;
    QVector<Quotient::Room*> rooms;

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

    static inline const auto SystemPrefix = QStringLiteral("im.quotient.");
    static inline const auto LegacyPrefix = QStringLiteral("org.qmatrixclient.");
};
using RoomGroups = QVector<RoomGroup>;

class RoomListModel;

class AbstractRoomOrdering : public QObject
{
        Q_OBJECT
    public:
        using Room = Quotient::Room;
        using Connection = Quotient::Connection;
        using groups_t = QVariantList;

        explicit AbstractRoomOrdering(RoomListModel* m);

    public: // Overridables
        /// Returns human-readable name of the room ordering
        virtual QString orderingName() const = 0;
        /// Returns human-readable room group caption
        virtual QVariant groupLabel(const RoomGroup& g) const = 0;
        /// Orders a group against a key of another group
        virtual bool groupLessThan(const RoomGroup& g1,
                                   const QVariant& g2key) const = 0;
        /// Orders two rooms within one group
        virtual bool roomLessThan(const QVariant& group,
                                  const Room* r1, const Room* r2) const = 0;

        /// Returns the full list of room groups
        virtual groups_t roomGroups(const Room* room) const = 0;
        /// Connects order updates to signals from a new Matrix connection
        virtual void connectSignals(Connection* connection) = 0;
        /// Connects order updates to signals from a new Matrix room
        virtual void connectSignals(Room* room) = 0;

    public:
        using groupLessThan_closure_t =
                std::function<bool(const RoomGroup&, const QVariant&)>;
        /// Returns a closure that invokes this->groupLessThan()
        groupLessThan_closure_t groupLessThanFactory() const;

        using roomLessThan_closure_t =
                std::function<bool(const Room*, const Room*)>;
        /// Returns a closure that invokes this->roomLessThan in a given group
        roomLessThan_closure_t roomLessThanFactory(const QVariant& group) const;

    protected slots:
        /// A facade for derived classes to trigger RoomListModel::updateGroups
        void updateGroups(Room* room);

    protected:
        RoomListModel* model() const;
};
