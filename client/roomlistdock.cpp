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

#include "roomlistdock.h"

#include <QtCore/QDebug>
#include <QtWidgets/QMenu>

#include "models/roomlistmodel.h"
#include "quaternionroom.h"

RoomListDock::RoomListDock(QWidget* parent)
    : QDockWidget("Rooms", parent)
{
    connection = nullptr;
    //setWidget(new QWidget());
    model = new RoomListModel(this);
    view = new QListView();
    view->setModel(model);
    connect( view, &QListView::activated, this, &RoomListDock::rowSelected );
    connect( view, &QListView::clicked, this, &RoomListDock::rowSelected);
    setWidget(view);

    contextMenu = new QMenu(this);
    joinAction = new QAction(tr("Join Room"), this);
    connect(joinAction, &QAction::triggered, this, &RoomListDock::menuJoinSelected);
    contextMenu->addAction(joinAction);
    leaveAction = new QAction(tr("Leave Room"), this);
    connect(leaveAction, &QAction::triggered, this, &RoomListDock::menuLeaveSelected);
    contextMenu->addAction(leaveAction);

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &RoomListDock::showContextMenu);
}

RoomListDock::~RoomListDock()
{
}

void RoomListDock::setConnection( QMatrixClient::Connection* connection )
{
    this->connection = connection;
    model->setConnection(connection);
}

void RoomListDock::rowSelected(const QModelIndex& index)
{
    emit roomSelected( model->roomAt(index.row()) );
}

void RoomListDock::showContextMenu(const QPoint& pos)
{
    QModelIndex index = view->indexAt(view->mapFromParent(pos));
    if( !index.isValid() )
        return;
    QuaternionRoom* room = model->roomAt(index.row());

    if( room->joinState() == QMatrixClient::JoinState::Join )
    {
        joinAction->setEnabled(false);
        leaveAction->setEnabled(true);
    }
    else
    {
        joinAction->setEnabled(true);
        leaveAction->setEnabled(false);
    }

    contextMenu->popup(mapToGlobal(pos));
}

void RoomListDock::menuJoinSelected()
{
    if (!connection)
        return;

    QModelIndex index = view->currentIndex();
    QuaternionRoom* room = model->roomAt(index.row());
    connection->joinRoom(room->id());
}

void RoomListDock::menuLeaveSelected()
{
    if (!connection)
        return;

    QModelIndex index = view->currentIndex();
    if( !index.isValid() )
        return;
    QuaternionRoom* room = model->roomAt(index.row());
    connection->leaveRoom(room);
}
