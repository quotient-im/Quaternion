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

#include <QtCore/QAbstractListModel>
#include <QtCore/QBasicTimer>

#include "../quaternionroom.h"

class MessageEventModel: public QAbstractListModel
{
        Q_OBJECT
        // The below property is marked constant because it only changes
        // when the whole model is reset (so anything that depends on the model
        // has to be re-calculated anyway).
        // XXX: A better way would be to make [Room::]Timeline a list model
        // itself, leaving only representation of the model to a client.
        Q_PROPERTY(QuaternionRoom* room MEMBER m_currentRoom CONSTANT)
    public:
        MessageEventModel(QObject* parent = nullptr);
        virtual ~MessageEventModel();

        void changeRoom(QuaternionRoom* room);

        int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        QHash<int, QByteArray> roleNames() const override;

    public slots:
        void onMessageShownChanged(QString eventId, bool shown);
        void markShownAsRead();

    protected:
        void timerEvent(QTimerEvent* event) override;

    private:
        QuaternionRoom* m_currentRoom;

        QVector<QMatrixClient::TimelineItem::index_t> indicesOnScreen;
        QuaternionRoom::rev_iter_t maybeReadMessage;
        QBasicTimer maybeReadTimer;

        void reStartShownTimer();
};
