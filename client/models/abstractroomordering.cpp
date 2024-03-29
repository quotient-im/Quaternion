/******************************************************************************
 * SPDX-FileCopyrightText: 2018-2019 QMatrixClient Project
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "abstractroomordering.h"

#include "roomlistmodel.h"

#include <functional>
using namespace std::placeholders;

AbstractRoomOrdering::AbstractRoomOrdering(RoomListModel* m)
    : QObject(m)
{ }

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

RoomListModel* AbstractRoomOrdering::model() const
{
    return static_cast<RoomListModel*>(parent());
}
