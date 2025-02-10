/******************************************************************************
 * SPDX-FileCopyrightText: 2018-2019 QMatrixClient Project
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
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

    private:
        QStringList tagsOrder;

        // Overrides

        QString orderingName() const override { return QStringLiteral("tag"); }
        QVariant groupLabel(const RoomGroup& g) const override;
        bool groupLessThan(const QVariant& g1key, const QVariant& g2key) const override;
        bool roomLessThan(const QVariant& groupKey,
                          const Room* r1, const Room* r2) const override;
        groups_t roomGroups(const Room* room) const override;
        void connectSignals(Connection* connection) override;
        void connectSignals(Room* room) override;
        void updateGroups(Room* room) override;
        QStringList getFilteredTags(const Room* room) const;

        static QStringList initTagsOrder();
};
