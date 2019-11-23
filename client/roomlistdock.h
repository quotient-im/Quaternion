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
#include <QtWidgets/QTreeView>
#include <QtCore/QStringListModel>
//#include <QtCore/QSortFilterProxyModel>

class MainWindow;
class RoomListModel;
class QuaternionRoom;

namespace Quotient {
    class Connection;
}

class RoomListDock : public QDockWidget
{
        Q_OBJECT
    public:
        explicit RoomListDock(MainWindow* parent = nullptr);

        void addConnection(Quotient::Connection* connection);

    public slots:
        void updateSortingMode();
        void setSelectedRoom(QuaternionRoom* room);

    signals:
        void roomSelected(QuaternionRoom* room);

    private slots:
        void rowSelected(const QModelIndex& index);
        void showContextMenu(const QPoint& pos);
        void addTagsSelected();
        void refreshTitle();

    private:
        QTreeView* view = nullptr;
        RoomListModel* model = nullptr;
        //        QSortFilterProxyModel* proxyModel;
        QMenu* roomContextMenu = nullptr;
        QMenu* groupContextMenu = nullptr;
        QAction* markAsReadAction = nullptr;
        QAction* addTagsAction = nullptr;
        QAction* joinAction = nullptr;
        QAction* leaveAction = nullptr;
        QAction* forgetAction = nullptr;
        QAction* deleteTagAction = nullptr;
        QAction* roomSettingsAction = nullptr;
        QVariant selectedGroupCache = {};
        QuaternionRoom* selectedRoomCache = nullptr;

        QVariant getSelectedGroup() const;
        QuaternionRoom* getSelectedRoom() const;
};
