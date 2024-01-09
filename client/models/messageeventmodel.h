/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2015 Felix Rohrbach <kde@fxrh.de>                        *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
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
            DateTimeRole,
            DateRole,
            EventGroupingRole,
            AuthorRole,
            AuthorHasAvatarRole,
            ContentRole,
            ContentTypeRole,
            HighlightRole,
            SpecialMarksRole,
            LongOperationRole,
            AnnotationRole,
            RefRole,
            ReactionsRole,
            EventClassNameRole,
        };

        explicit MessageEventModel(QObject* parent = nullptr);

        QuaternionRoom* room() const;
        void changeRoom(QuaternionRoom* room);

        int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        QVariant data(const QModelIndex& idx, int role = Qt::DisplayRole) const override;
        QHash<int, QByteArray> roleNames() const override;
        int findRow(const QString& id, bool includePending = false) const;

        Q_INVOKABLE QColor fadedBackColor(QColor unfadedColor,
                                          qreal fadeRatio = 0.5) const;

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
