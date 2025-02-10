/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2015 Felix Rohrbach <kde@fxrh.de>              *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#pragma once

// QSslError is used in a signal container parameter and needs to be complete
// for moc to generate stuff since Qt 6
#include <QtNetwork/QSslError>
#include <QtWidgets/QMainWindow>

#include <Quotient/accountregistry.h>
#include <Quotient/uriresolver.h>

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
class QNetworkProxy;
class QAuthenticator;

class MainWindow: public QMainWindow, public Quotient::UriResolverBase {
        Q_OBJECT
    public:
        using Connection = Quotient::Connection;

        MainWindow();
        ~MainWindow() override;

        void addConnection(Connection* c);
        void dropConnection(Connection* c);

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
        void showStatusMessage(const QString& message, int timeout = 0);
        QFuture<QString> logout(Connection* c);

    private slots:
        void invokeLogin();

        void reloginNeeded(Connection* c, const QString& message = {});
        void networkError(Connection* c);
        void sslErrors(const QPointer<QNetworkReply>& reply,
                       const QList<QSslError>& errors);
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
        Quotient::AccountRegistry* accountRegistry =
            new Quotient::AccountRegistry(this);
        QVector<Connection*> logoutOnExit;

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
        QAction* confirmLinksAction = nullptr;

        SystemTrayIcon* systemTrayIcon = nullptr;

        // FIXME: This will be a problem when we get ability to show
        // several rooms at once.
        QuaternionRoom* currentRoom = nullptr;

        void createMenu();
        QAction* addUiOptionCheckbox(QMenu* parent, const QString& text,
            const QString& statusTip, const QString& settingsKey,
            bool defaultValue = false);
        void showInitialLoadIndicator();
        void updateLoadingStatus(int accountsStillLoading);
        void firstSyncOver(const Connection *c);
        void loadSettings();
        void saveSettings() const;
        void doOpenLoginDialog(LoginDialog* dialog);
        Connection* chooseConnection(Connection* connection,
                                     const QString& prompt);
        void showMillisToRecon(Connection* c);

        std::pair<QByteArray, bool> loadAccessToken(
            const Quotient::AccountSettings& account);

        /// Get the default connection to perform actions
        /*!
         * \return the connection of the current room; or, if there's only
         *         one connection, that connection; failing that, nullptr
         */
        Connection* getDefaultConnection() const;

        void closeEvent(QCloseEvent* event) override;
};
