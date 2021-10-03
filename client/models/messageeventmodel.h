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
        Q_PROPERTY(QuaternionRoom* room READ room NOTIFY roomChanged)
        Q_PROPERTY(int readMarkerVisualIndex READ readMarkerVisualIndex NOTIFY readMarkerUpdated)
    public:
        enum EventRoles {
            EventTypeRole = Qt::UserRole + 1,
            EventIdRole,
            TimeRole,
            DateRole,
            EventGroupingRole,
            AuthorRole,
            ContentRole,
            ContentTypeRole,
            HighlightRole,
            SpecialMarksRole,
            LongOperationRole,
            AnnotationRole,
            RefRole,
            ReactionsRole,
            EventResolvedTypeRole,
        };

        explicit MessageEventModel(QObject* parent = nullptr);

        QuaternionRoom* room() const;
        void changeRoom(QuaternionRoom* room);

        int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        QVariant data(const QModelIndex& idx, int role = Qt::DisplayRole) const override;
        QHash<int, QByteArray> roleNames() const override;
        int findRow(const QString& id, bool includePending = false) const;

    signals:
        void roomChanged();
        /// This is different from Room::readMarkerMoved() in that it is also
        /// emitted when the room or the last read event is first shown
        void readMarkerUpdated();

    private slots:
        int refreshEvent(const QString& eventId);
        void refreshRow(int row);
        void incomingEvents(Quotient::RoomEventsRange events, int atIndex);

    private:
        QuaternionRoom* m_currentRoom = nullptr;
        int readMarkerVisualIndex() const;
        bool movingEvent = false;

        int timelineBaseIndex() const;
        QDateTime makeMessageTimestamp(const QuaternionRoom::rev_iter_t& baseIt) const;
        static QString renderDate(const QDateTime& timestamp);
        bool isUserActivityNotable(const QuaternionRoom::rev_iter_t& baseIt) const;

        void refreshLastUserEvents(int baseTimelineRow);
        void refreshEventRoles(int row, const QVector<int>& roles = {});
};

namespace EventGrouping {
Q_NAMESPACE

enum Mark {
    KeepPreviousGroup = 0,
    ShowAuthor = 1,
    ShowDateAndAuthor = 2
};
Q_ENUM_NS(Mark)

}
