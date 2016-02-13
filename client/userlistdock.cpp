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

#include "userlistdock.h"

#include <QtWidgets/QTableView>
#include <QtWidgets/QHeaderView>

#include "lib/connection.h"
#include "lib/room.h"
#include "models/userlistmodel.h"

UserListDock::UserListDock(QWidget* parent)
    : QDockWidget("Users", parent)
{
    m_view = new QTableView();
    m_view->setShowGrid(false);
    m_view->horizontalHeader()->setStretchLastSection(true);
    m_view->horizontalHeader()->setVisible(false);
    m_view->verticalHeader()->setVisible(false);
    setWidget(m_view);

    m_model = new UserListModel();
    m_view->setModel(m_model);
}

UserListDock::~UserListDock()
{
}

void UserListDock::setConnection(QMatrixClient::Connection* connection)
{
    m_model->setConnection(connection);
}

void UserListDock::setRoom(QMatrixClient::Room* room)
{
    m_model->setRoom(room);
}