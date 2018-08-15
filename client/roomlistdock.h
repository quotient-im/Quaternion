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
#include <QSortFilterProxyModel>

class RoomListModel;
class QuaternionRoom;

namespace QMatrixClient {
    class Connection;
}

class RoomListDock : public QDockWidget
{
        Q_OBJECT
    public:
        explicit RoomListDock(QWidget* parent = nullptr);

        void addConnection(QMatrixClient::Connection* connection);

    public slots:
        void updateSortingMode();

    signals:
        void roomSelected(QuaternionRoom* room);

    private slots:
        void rowSelected(const QModelIndex& index);
        void showContextMenu(const QPoint& pos);
        void menuMarkReadSelected();
        void addTagsSelected();
        void menuJoinSelected();
        void menuLeaveSelected();
        void menuForgetSelected();
        void refreshTitle();

    private:
        QTreeView* view;
        RoomListModel* model;
//        QSortFilterProxyModel* proxyModel;
        QMenu* contextMenu;
        QAction* markAsReadAction;
        QAction* addTagsAction;
        QAction* joinAction;
        QAction* leaveAction;
        QAction* forgetAction;
        QVariant selectedGroupCache;
        QuaternionRoom* selectedRoomCache;

        QVariant getSelectedGroup() const;
        QuaternionRoom* getSelectedRoom() const;
};
