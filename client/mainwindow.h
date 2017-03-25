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

#include <QtWidgets/QMainWindow>

class RoomListDock;
class UserListDock;
class ChatRoomWidget;
class QuaternionConnection;
class SystemTray;

class QAction;
class QMenu;
class QMenuBar;
class QSystemTrayIcon;
class QMovie;
class QLabel;

class MainWindow: public QMainWindow
{
        Q_OBJECT
    public:
        MainWindow();
        virtual ~MainWindow();

        void enableDebug();
        void activity();

        void setConnection(QuaternionConnection* newConnection);

    protected:
        virtual void closeEvent(QCloseEvent* event) override;

    private slots:
        void initialize();
        void initialSync();
        void getNewEvents();
        void gotEvents();
        void loggedOut(const QString& message = QString());

        void connectionError(QString error);

        void showJoinRoomDialog();
        void showLoginWindow(const QString& statusMessage = QString());
        void logout();

    private:
        RoomListDock* roomListDock;
        UserListDock* userListDock;
        ChatRoomWidget* chatRoomWidget;
        QuaternionConnection* connection;

        QMovie* busyIndicator;
        QLabel* busyLabel;

        QAction* loginAction;
        QAction* logoutAction;

        SystemTray* systemTray;

        void createMenu();
        void invokeLogin();
        void loadSettings();
        void saveSettings() const;
};
