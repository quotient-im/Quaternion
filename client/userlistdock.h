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

#ifndef USERLISTDOCK_H
#define USERLISTDOCK_H

#include <QtWidgets/QDockWidget>

namespace QMatrixClient
{
    class Connection;
    class Room;
}

class UserListModel;
class QTableView;

class UserListDock: public QDockWidget
{
        Q_OBJECT
    public:
        UserListDock(QWidget* parent=0);
        virtual ~UserListDock();

        void setConnection( QMatrixClient::Connection* connection );
        void setRoom( QMatrixClient::Room* room );

    private:
        QTableView* m_view;
        UserListModel* m_model;
};

#endif // USERLISTDOCK_H

