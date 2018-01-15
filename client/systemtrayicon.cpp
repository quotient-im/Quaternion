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

#include "systemtrayicon.h"

#include <QtWidgets/QWidget>

#include "lib/connection.h"
#include "lib/room.h"

SystemTrayIcon::SystemTrayIcon(QWidget* parent)
    : QSystemTrayIcon(parent)
    , m_parent(parent)
{
    setIcon(QIcon(":/icon.png"));
}

void SystemTrayIcon::newRoom(QMatrixClient::Room* room)
{
    connect(room, &QMatrixClient::Room::highlightCountChanged,
            this, &SystemTrayIcon::highlightCountChanged);
}

void SystemTrayIcon::highlightCountChanged(QMatrixClient::Room* room)
{
    if( room->highlightCount() > 0 )
    {
        showMessage(tr("Highlight!"), tr("%1: %2 highlight(s)").arg(room->displayName()).arg(room->highlightCount()));
        m_parent->activateWindow();
    }
}
