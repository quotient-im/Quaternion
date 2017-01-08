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

#pragma once

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
        UserListDock(QWidget* parent = nullptr);
        virtual ~UserListDock();

    void setRoom( QMatrixClient::Room* room );

    private:
        QTableView* m_view;
        UserListModel* m_model;
};
