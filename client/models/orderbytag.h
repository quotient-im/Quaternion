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

#include "abstractroomordering.h"

// TODO: When the library l10n is enabled, these two should go down to it
QString tagToCaption(const QString& tag);
QString captionToTag(const QString& caption);

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

        static QStringList initTagsOrder();
};
