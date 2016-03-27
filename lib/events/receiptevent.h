/******************************************************************************
 * Copyright (C) 2016 Felix Rohrbach <kde@fxrh.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef QMATRIXCLIENT_RECEIPTEVENT_H
#define QMATRIXCLIENT_RECEIPTEVENT_H

#include "event.h"

#include <QtCore/QStringList>

namespace QMatrixClient
{
    class Receipt
    {
        public:
            QString eventId;
            QString userId;
            QDateTime timestamp;

            Receipt(QString event, QString user, QDateTime time);
    };

    class ReceiptEvent: public Event
    {
        public:
            ReceiptEvent();
            virtual ~ReceiptEvent();

            QList<Receipt> receiptsForEvent(QString eventId) const;

            QStringList events() const;

            static ReceiptEvent* fromJson(const QJsonObject& obj);

        private:
            class Private;
            Private* d;
    };
}

#endif // QMATRIXCLIENT_RECEIPTEVENT_H
