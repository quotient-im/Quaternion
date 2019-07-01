/**************************************************************************
 *                                                                        *
 * Copyright (C) 2015 Felix Rohrbach <kde@fxrh.de>                        *
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

#include <QtCore/QAbstractListModel>

class MessageEventModel: public QAbstractListModel
{
        Q_OBJECT
    public:
        enum EventRoles {
            EventTypeRole = Qt::UserRole + 1,
            EventIdRole,
            TimeRole,
            SectionRole,
            AboveSectionRole,
            AuthorRole,
            AboveAuthorRole,
            ContentRole,
            ContentTypeRole,
            HighlightRole,
            ReadMarkerRole,
            SpecialMarksRole,
            LongOperationRole,
            AnnotationRole,
            UserHueRole,
            RefRole,
            EventResolvedTypeRole,
        };

        explicit MessageEventModel(QObject* parent = nullptr);

        void changeRoom(QuaternionRoom* room);

        int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        QVariant data(const QModelIndex& idx, int role = Qt::DisplayRole) const override;
        QHash<int, QByteArray> roleNames() const override;

    private slots:
        int refreshEvent(const QString& eventId);
        void refreshRow(int row);

    private:
        QuaternionRoom* m_currentRoom = nullptr;
        QString lastReadEventId;
        int rowBelowInserted = -1;
        bool movingEvent = false;

        int timelineBaseIndex() const;
        QDateTime makeMessageTimestamp(const QuaternionRoom::rev_iter_t& baseIt) const;
        QString renderDate(const QDateTime& timestamp) const;
        bool isUserActivityNotable(const QuaternionRoom::rev_iter_t& baseIt) const;

        void refreshLastUserEvents(int baseTimelineRow);
        void refreshEventRoles(int row, const QVector<int>& roles = {});
        int refreshEventRoles(const QString& id, const QVector<int>& roles = {});
};
