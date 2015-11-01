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

#include "roomlistdock.h"

#include <QtCore/QDebug>

RoomListDock::RoomListDock(QWidget* parent)
    : QDockWidget("Rooms", parent)
{
    connection = 0;
    //setWidget(new QWidget());
    model = new QStringListModel(this);
    view = new QListView();
    view->setModel(model);
    connect( view, &QListView::activated, this, &RoomListDock::rowSelected );
    setWidget(view);
}

RoomListDock::~RoomListDock()
{
}

void RoomListDock::setConnection( QMatrixClient::Connection* connection )
{
    this->connection = connection;
    QStringList rooms;
    for( const QString& room : connection->roomMap().keys() )
    {
        QString alias = connection->roomMap().value(room)->alias();
        if( alias.isEmpty() )
            alias = room;
        rooms.append( alias );
    }
    qDebug() << rooms;
    model->setStringList( rooms );
    connect( connection, &QMatrixClient::Connection::newRoom, this, &RoomListDock::newRoom );
}

void RoomListDock::rowSelected(const QModelIndex& index)
{
    QString id = connection->roomMap().keys().at(index.row());
    emit roomSelected( connection->roomMap().value(id) );
}

void RoomListDock::newRoom(QMatrixClient::Room* room)
{
    QStringList rooms = model->stringList();
    rooms << room->alias();
    model->setStringList(rooms);
}