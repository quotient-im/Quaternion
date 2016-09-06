/**************************************************************************
 *                                                                        *
 * Copyright (C) 2016 Felix Rohrbach <kde@fxrh.de>                        *
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

#include "systemtray.h"

#include <QtWidgets/QWidget>

#include "lib/connection.h"
#include "lib/room.h"

SystemTray::SystemTray(QWidget* parent)
    : QSystemTrayIcon(parent)
    , m_connection(nullptr)
    , m_parent(parent)
{
    setIcon(QIcon(":/icon.png"));
}

void SystemTray::setConnection(QMatrixClient::Connection* connection)
{
    if( m_connection )
    {
        m_connection->disconnect(this);
    }
    m_connection = connection;
    if( m_connection )
    {
        connect(m_connection, &QMatrixClient::Connection::newRoom, this, &SystemTray::newRoom);
    }
}

void SystemTray::newRoom(QMatrixClient::Room* room)
{
    connect(room, &QMatrixClient::Room::highlightCountChanged, this, &SystemTray::highlightCountChanged);
}

void SystemTray::highlightCountChanged(QMatrixClient::Room* room)
{
    if( room->highlightCount() > 0 )
    {
        showMessage(tr("Highlight!"), tr("%1: %2 highlight(s)").arg(room->displayName()).arg(room->highlightCount()));
        m_parent->raise();
    }
}
