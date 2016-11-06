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

#ifndef LOGMESSAGEMODEL_H
#define LOGMESSAGEMODEL_H

#include "../quaternionroom.h"

#include <QtCore/QAbstractListModel>
#include <QtCore/QModelIndex>

class Message;

class MessageEventModel: public QAbstractListModel
{
        Q_OBJECT
        Q_PROPERTY(QuaternionRoom* room MEMBER m_currentRoom CONSTANT)
        Q_PROPERTY(int lastShownIndex MEMBER lastShownIndex NOTIFY lastShownIndexChanged)
        Q_PROPERTY(int readMarkerIndex MEMBER m_readMarkerIndex NOTIFY readMarkerIndexChanged)
    public:
        MessageEventModel(QObject* parent = nullptr);
        virtual ~MessageEventModel();

        void changeRoom(QuaternionRoom* room);

        int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        QHash<int, QByteArray> roleNames() const override;

        bool awaitingMarkRead();

    signals:
        void lastShownIndexChanged(int newValue);
        void readMarkerIndexChanged(int newValue);

    public slots:
        void markShownAsRead();
        void updateReadMarkerIndex();

    private:
        QuaternionRoom* m_currentRoom;
        int lastShownIndex;
        int m_readMarkerIndex;
};

#endif // LOGMESSAGEMODEL_H
