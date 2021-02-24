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

#include "accountregistry.h"

#include <uriresolver.h>

#include <QtWidgets/QMainWindow>

namespace Quotient {
    class Room;
    class Connection;
    class AccountSettings;
}

class RoomListDock;
class UserListDock;
class ChatRoomWidget;
class SystemTrayIcon;
class QuaternionRoom;
class LoginDialog;

class QAction;
class QMenu;
class QMenuBar;
class QSystemTrayIcon;
class QMovie;
class QLabel;
class QLineEdit;

class QNetworkReply;
class QSslError;
class QNetworkProxy;
class QAuthenticator;

class MainWindow: public QMainWindow, public Quotient::UriResolverBase {
        Q_OBJECT
    public:
        using Connection = Quotient::Connection;

        MainWindow();
        ~MainWindow() override;

        void enableDebug();

        void addConnection(Connection* c, const QString& deviceName);
        void dropConnection(Connection* c);

        ChatRoomWidget* getChatRoomWidget() const;

        // For openUserInput()
        enum : bool { NoRoomJoining = false, ForJoining = true };

    public slots:
        /// Open non-empty id or URI using the specified action hint
        /*! Asks the user to choose the connection if necessary */
        void openResource(const QString& idOrUri, const QString& action = {});
        /// Open a dialog to enter the resource id/URI and then navigate to it
        void openUserInput(bool forJoining = NoRoomJoining);
        /// Open/focus the room settings dialog
        /*! If \p r is empty, the currently open room is used */
        void openRoomSettings(QuaternionRoom* r = nullptr);
        void selectRoom(Quotient::Room* r);
        void logout(Connection* c);

    private slots:
        void invokeLogin();

        void loginError(Connection* c, const QString& message = {});
        void networkError(Connection* c);
        void sslErrors(QNetworkReply* reply, const QList<QSslError>& errors);
        void proxyAuthenticationRequired(const QNetworkProxy& /* unused */,
                                         QAuthenticator* auth);

        void showLoginWindow(const QString& statusMessage = {});
        void showLoginWindow(const QString& statusMessage,
                             const QString& userId);
        void showAboutWindow();

        // UriResolverBase overrides
        Quotient::UriResolveResult visitUser(Quotient::User* user,
                                             const QString& action) override;
        void visitRoom(Quotient::Room* room, const QString& eventId) override;
        void joinRoom(Quotient::Connection* account,
                      const QString& roomAliasOrId,
                      const QStringList& viaServers = {}) override;
        bool visitNonMatrix(const QUrl& url) override;

    private:
        AccountRegistry accountRegistry;
        QVector<Connection*> logoutOnExit;
        QVector<Connection*> firstSyncing;

        RoomListDock* roomListDock = nullptr;
        UserListDock* userListDock = nullptr;
        ChatRoomWidget* chatRoomWidget = nullptr;

        QMovie* busyIndicator = nullptr;
        QLabel* busyLabel = nullptr;

        QMenu* connectionMenu = nullptr;
        QMenu* logoutMenu = nullptr;
        QAction* openRoomAction = nullptr;
        QAction* roomSettingsAction = nullptr;
        QAction* createRoomAction = nullptr;
        QAction* dcAction = nullptr;
        QAction* joinAction = nullptr;

        SystemTrayIcon* systemTrayIcon = nullptr;

        // FIXME: This will be a problem when we get ability to show
        // several rooms at once.
        QuaternionRoom* currentRoom = nullptr;

        void createMenu();
        QAction* addUiOptionCheckbox(QMenu* parent, const QString& text,
            const QString& statusTip, const QString& settingsKey,
            bool defaultValue = false);
        void showFirstSyncIndicator();
        void firstSyncOver(Connection* c);
        void loadSettings();
        void saveSettings() const;
        void doOpenLoginDialog(LoginDialog* dialog);
        QByteArray loadAccessToken(const Quotient::AccountSettings& account);
        QByteArray loadAccessTokenFromFile(const Quotient::AccountSettings& account);
        QByteArray loadAccessTokenFromKeyChain(const Quotient::AccountSettings &account);
        bool saveAccessToken(const Quotient::AccountSettings& account,
                             const QByteArray& accessToken);
        bool saveAccessTokenToFile(const Quotient::AccountSettings& account,
                                   const QByteArray& accessToken);
        bool saveAccessTokenToKeyChain(const Quotient::AccountSettings& account,
                                       const QByteArray& accessToken, bool writeToFile = true);
        Connection* chooseConnection(Connection* connection,
                                     const QString& prompt);
        void showMillisToRecon(Connection* c);

        /// Get the default connection to perform actions
        /*!
         * \return the connection of the current room; or, if there's only
         *         one connection, that connection; failing that, nullptr
         */
        Connection* getDefaultConnection() const;

        void closeEvent(QCloseEvent* event) override;
};
