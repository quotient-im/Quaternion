/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2015 Felix Rohrbach <kde@fxrh.de>              *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
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
        void deleteConnection(Quotient::Connection* connection);

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
        QAction* roomPermalinkAction = nullptr;
        QVariant selectedGroupCache = {};
        QuaternionRoom* selectedRoomCache = nullptr;

        QVariant getSelectedGroup() const;
        QuaternionRoom* getSelectedRoom() const;
};
