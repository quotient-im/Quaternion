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

#ifndef QMATRIXCLIENT_LOGMESSAGE_H
#define QMATRIXCLIENT_LOGMESSAGE_H

#include <QtCore/QString>

namespace QMatrixClient
{
    class LogMessage
    {
        public:
            enum MessageType{ UserMessage, StatusMessage };

            LogMessage( MessageType type, QString message, QString author=QString() );
            virtual ~LogMessage();

            MessageType type() const;
            QString message() const;
            QString author() const;

        private:
            class Private;
            Private* d;
    };
}

#endif // QMATRIXCLIENT_LOGMESSAGE_H