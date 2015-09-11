/******************************************************************************
 * Copyright (C) 2015 Felix Rohrbach <kde@fxrh.de>
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

#ifndef QMATRIXCLIENT_STATE_H
#define QMATRIXCLIENT_STATE_H

#include <QtCore/QString>
#include <QtCore/QJsonObject>

namespace QMatrixClient
{
    class Event;

    /**
     * Wraps an event that is a state
     */
    class State
    {
        public:
            State(Event* event);
            virtual ~State();

            Event* event() const;
            QString stateKey() const;
            QString replacesState() const;

            static State* fromJson(const QJsonObject& obj);

        private:
            class Private;
            Private* d;
    };
}

#endif // QMATRIXCLIENT_STATE_H