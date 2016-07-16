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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets/QMainWindow>
#include <QSettings>

#include "lib/room.h"
#include "lib/jobs/basejob.h"

class RoomListDock;
class UserListDock;
class ChatRoomWidget;
class QuaternionConnection;
class SystemTray;

class QAction;
class QMenu;
class QMenuBar;
class QSystemTrayIcon;

class MainWindow: public QMainWindow
{
        Q_OBJECT
    public:
        MainWindow();
        virtual ~MainWindow();

        void enableDebug();

    private slots:
        void initialize();
        void getNewEvents();
        void gotEvents();

        void connectionError(QString error);

    protected:
        virtual void closeEvent(QCloseEvent* event) override;

    private slots:
        void showJoinRoomDialog();
        void logout();

    private:
        RoomListDock* roomListDock;
        UserListDock* userListDock;
        ChatRoomWidget* chatRoomWidget;
        QuaternionConnection* connection;

        QMenuBar* menuBar;
        QMenu* connectionMenu;
        QMenu* roomMenu;

        QAction *logoutAction;
        QAction* quitAction;
        QAction* joinRoomAction;

        SystemTray* systemTray;

        QSettings *settings;
};

#endif // MAINWINDOW_H
