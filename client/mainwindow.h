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

struct Locator
{
    QMatrixClient::Connection* account = nullptr;
    QString identifier; //< Room id, room alias, or user id
};

class MainWindow: public QMainWindow
{
        Q_OBJECT
    public:
        using Connection = QMatrixClient::Connection;

        MainWindow();
        ~MainWindow() override;

        void enableDebug();

        void addConnection(Connection* c, const QString& deviceName);
        void dropConnection(Connection* c);

        ChatRoomWidget* getChatRoomWidget() const;

        void resolveLocator(const Locator& l, const QString& action = {});

    public slots:
        void resolveResource(const QString& idOrUri, const QString& action = {});
        void selectRoom(QMatrixClient::Room* r);

    private slots:
        void invokeLogin();
        void joinRoom(const QString& roomAlias = {});
        void directChat(const QString& userId = {});
        void getNewEvents(Connection* c);
        void gotEvents(Connection* c);

        void loginError(Connection* c, const QString& message = {});
        void networkError(Connection* c);
        void sslErrors(QNetworkReply* reply, const QList<QSslError>& errors);
        void proxyAuthenticationRequired(const QNetworkProxy& /* unused */,
                                         QAuthenticator* auth);

        void showLoginWindow(const QString& statusMessage = {});
        void showLoginWindow(const QString& statusMessage,
            QMatrixClient::AccountSettings& reloginConnection);
        void showAboutWindow();
        void logout(Connection* c);

    private:
        enum CompletionType {
            None,
            Room,
            User
        };

        QVector<Connection*> connections;
        QVector<Connection*> logoutOnExit;

        RoomListDock* roomListDock = nullptr;
        UserListDock* userListDock = nullptr;
        ChatRoomWidget* chatRoomWidget = nullptr;

        QMovie* busyIndicator = nullptr;
        QLabel* busyLabel = nullptr;

        QMenu* connectionMenu = nullptr;
        QAction* accountListGrowthPoint = nullptr;
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
        QAction* addTimelineOptionCheckbox(QMenu* parent,
            const QString& text, const QString& statusTip,
            const QString& settingsKey, bool defaultValue = false);
        void showFirstSyncIndicator();
        void loadSettings();
        void saveSettings() const;
        void processLogin(LoginDialog& dialog);
        QByteArray loadAccessToken(const QMatrixClient::AccountSettings& account);
        QByteArray loadAccessTokenFromFile(const QMatrixClient::AccountSettings& account);
        QByteArray loadAccessTokenFromKeyChain(const QMatrixClient::AccountSettings &account);
        bool saveAccessToken(const QMatrixClient::AccountSettings& account,
                             const QByteArray& accessToken);
        bool saveAccessTokenToFile(const QMatrixClient::AccountSettings& account,
                                   const QByteArray& accessToken);
        bool saveAccessTokenToKeyChain(const QMatrixClient::AccountSettings& account,
                                       const QByteArray& accessToken);
        Connection* chooseConnection(Connection* connection,
                                     const QString& prompt);
        void showMillisToRecon(Connection* c);
        /// Asks a user to pick an account and enter the Matrix identifier
        /*!
         * The identifier can be a room id or alias (to join/open rooms) or
         * a user MXID (e.g., to open direct chats or user profiles).
         * \param initialConn initially selected account, if there are several
         * \param completionType - type of completion
         * \param prompt - dialog box title
         * \param label - the text next to the identifier field (e.g., "User ID")
         * \param actionName - the text on the accepting button
         */
        Locator obtainIdentifier(Connection* initialConn,
                                 QFlags<CompletionType> completionType,
                                 const QString& prompt, const QString& label,
                                 const QString& actionName);
        void setCompleter(QLineEdit* edit, Connection* connection,
                          QFlags<CompletionType> type);

        void closeEvent(QCloseEvent* event) override;
};
