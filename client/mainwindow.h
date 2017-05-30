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

namespace QMatrixClient {
    class Connection;
}

class RoomListDock;
class UserListDock;
class ChatRoomWidget;
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
        using Connection = QMatrixClient::Connection;

        MainWindow();
        virtual ~MainWindow();

        void enableDebug();

        void addConnection(Connection* c);
        void dropConnection(Connection* c);

        ChatRoomWidget* getChatRoomWidget() const;

    protected:
        void closeEvent(QCloseEvent* event) override;

    private slots:
        void initialize();
        void onConnected(Connection* c);
        void joinRoom(const QString& roomAlias = {});
        void getNewEvents(Connection* c);
        void gotEvents(Connection* c);
        void loginError(Connection* c, const QString& message = {});

        void networkError(Connection* c);

        void showLoginWindow(const QString& statusMessage = {});
        void logout(Connection* c);
    private:
        QList<Connection*> connections;

        RoomListDock* roomListDock = nullptr;
        UserListDock* userListDock = nullptr;
        ChatRoomWidget* chatRoomWidget = nullptr;

        QMovie* busyIndicator = nullptr;
        QLabel* busyLabel = nullptr;

        QMenu* connectionMenu = nullptr;
        QAction* accountListGrowthPoint = nullptr;

        SystemTray* systemTray = nullptr;

        void createMenu();
        void invokeLogin();
        void loadSettings();
        void saveSettings() const;
        void showMillisToRecon(Connection* c);
};
