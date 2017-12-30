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

#include "mainwindow.h"

#include "roomlistdock.h"
#include "userlistdock.h"
#include "chatroomwidget.h"
#include "logindialog.h"
#include "networkconfigdialog.h"
#include "systemtray.h"

#include "lib/jobs/joinroomjob.h"
#include "lib/connection.h"
#include "lib/networkaccessmanager.h"
#include "lib/settings.h"

#include <QtCore/QTimer>
#include <QtCore/QDebug>
#include <QtCore/QStandardPaths>
#include <QtCore/QStringBuilder>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QAuthenticator>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtGui/QMovie>
#include <QtGui/QCloseEvent>

using QMatrixClient::NetworkAccessManager;

MainWindow::MainWindow()
{
    Connection::setRoomType<QuaternionRoom>();

    // Bind callbacks to signals from NetworkAccessManager

    auto nam = NetworkAccessManager::instance();
    connect(nam, &QNetworkAccessManager::proxyAuthenticationRequired,
        this, &MainWindow::proxyAuthenticationRequired);
    connect(nam, &QNetworkAccessManager::sslErrors,
            this, &MainWindow::sslErrors);

    setWindowIcon(QIcon(":/icon.png"));

    roomListDock = new RoomListDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, roomListDock);
    userListDock = new UserListDock(this);
    addDockWidget(Qt::RightDockWidgetArea, userListDock);
    chatRoomWidget = new ChatRoomWidget(this);
    setCentralWidget(chatRoomWidget);
    connect( chatRoomWidget, &ChatRoomWidget::joinCommandEntered,
             this, [=] (QString roomIdOrAlias)  { joinRoom(roomIdOrAlias); });
    connect( roomListDock, &RoomListDock::roomSelected,
             this, &MainWindow::selectRoom);
    connect( chatRoomWidget, &ChatRoomWidget::showStatusMessage, statusBar(), &QStatusBar::showMessage );

    createMenu();
    systemTray = new SystemTray(this);
    systemTray->show();

    busyIndicator = new QMovie(":/busy.gif");
    busyLabel = new QLabel(this);
    busyLabel->setMovie(busyIndicator);
    busyLabel->hide();
    statusBar()->setSizeGripEnabled(false);
    statusBar()->addPermanentWidget(busyLabel);
    loadSettings();

    QTimer::singleShot(0, this, SLOT(invokeLogin()));
}

ChatRoomWidget* MainWindow::getChatRoomWidget() const
{
   return chatRoomWidget;
}

void MainWindow::createMenu()
{
    // Connection menu
    connectionMenu = menuBar()->addMenu(tr("&Accounts"));

    connectionMenu->addAction(tr("&Login..."), this, [=]{ showLoginWindow(); } );

    connectionMenu->addSeparator();
    // Account submenus will be added in this place - see addConnection()
    accountListGrowthPoint = connectionMenu->addSeparator();

    connectionMenu->addAction(tr("&Quit"),
        qApp, &QApplication::closeAllWindows, QKeySequence::Quit);

    // View menu
    auto viewMenu = menuBar()->addMenu(tr("&View"));

    viewMenu->addAction(roomListDock->toggleViewAction());
    viewMenu->addAction(userListDock->toggleViewAction());

    // Room menu
    auto roomMenu = menuBar()->addMenu(tr("&Room"));

    roomMenu->addAction(tr("&Join Room..."), [=]{ joinRoom(); } );
    roomMenu->addSeparator();
    roomMenu->addAction(tr("&Close current room"),
        [this] { selectRoom(nullptr); }, QKeySequence::Close);

    // Settings menu
    auto settingsMenu = menuBar()->addMenu(tr("&Settings"));

    settingsMenu->addAction(tr("Configure &network proxy..."), [this]
    {
        static QPointer<NetworkConfigDialog> dlg;
        if (!dlg)
        {
            dlg = new NetworkConfigDialog(this);
            dlg->setModal(false);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
        }

        dlg->reactivate();
    });
}

void MainWindow::loadSettings()
{
    QMatrixClient::SettingsGroup sg("UI/MainWindow");
    if (sg.contains("normal_geometry"))
        setGeometry(sg.value("normal_geometry").toRect());
    if (sg.value("maximized").toBool())
        showMaximized();
    if (sg.contains("window_parts_state"))
        restoreState(sg.value("window_parts_state").toByteArray());
}

void MainWindow::saveSettings() const
{
    QMatrixClient::SettingsGroup sg("UI/MainWindow");
    sg.setValue("normal_geometry", normalGeometry());
    sg.setValue("maximized", isMaximized());
    sg.setValue("window_parts_state", saveState());
    sg.sync();
}

inline QString accessTokenFileName(const QMatrixClient::AccountSettings& account)
{
    QString fileName = account.userId();
    fileName.replace(':', '_');
    return QStandardPaths::writableLocation(
#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
                QStandardPaths::AppLocalDataLocation
#else
                QStandardPaths::DataLocation
#endif
                ) % '/' % fileName;
}

QByteArray MainWindow::loadAccessToken(const QMatrixClient::AccountSettings& account)
{
    QFile accountTokenFile { accessTokenFileName(account) };
    if (accountTokenFile.open(QFile::ReadOnly))
    {
        if (accountTokenFile.size() < 1024)
            return accountTokenFile.readAll();

        qWarning() << "File" << accountTokenFile.fileName()
                   << "is" << accountTokenFile.size()
                   << "bytes long - too long for a token, ignoring it.";
    }
    qWarning() << "Could not open access token file"
               << accountTokenFile.fileName();

    return {};
}

bool MainWindow::saveAccessToken(const QMatrixClient::AccountSettings& account,
                                 const QByteArray& accessToken)
{
    // (Re-)Make a dedicated file for access_token.
    QFile accountTokenFile { accessTokenFileName(account) };
    accountTokenFile.remove(); // Just in case

    auto fileDir = QFileInfo(accountTokenFile).dir();
    if (!( (fileDir.exists() || fileDir.mkpath(".")) &&
            accountTokenFile.open(QFile::WriteOnly) ))
    {
        QMessageBox::warning(this,
            tr("Couldn't open a file to save access token"),
            tr("Quaternion couldn't open a file to write the"
               " access token to. You're logged in but will have"
               " to provide your password again when you restart"
               " the application."), QMessageBox::Close);
    } else {
        // Try to restrict access rights to the file. The below is useless
        // on Windows: FAT doesn't control access at all and NTFS is
        // incompatible with the UNIX perms model used by Qt. If the attempt
        // didn't have the effect, at least ask the user if it's fine to save
        // the token to a file readable by others.
        // TODO: use system-specific API to ensure proper access.
        if ((accountTokenFile.setPermissions(
                    QFile::ReadOwner|QFile::WriteOwner) &&
            !(accountTokenFile.permissions() &
                    (QFile::ReadGroup|QFile::ReadOther)))
                ||
            QMessageBox::warning(this,
                    tr("Couldn't set access token file permissions"),
                    tr("Quaternion couldn't restrict permissions on the"
                       " access token file. Do you still want to save"
                       " the access token to it?"),
                    QMessageBox::Yes|QMessageBox::No
                ) == QMessageBox::Yes)
        {
            accountTokenFile.write(accessToken);
            return true;
        }
    }
    return false;
}

void MainWindow::enableDebug()
{
    chatRoomWidget->enableDebug();
}

void MainWindow::addConnection(Connection* c, const QString& deviceName)
{
    Q_ASSERT_X(c, __FUNCTION__, "Attempt to add a null connection");

    connections.push_back(c);

    roomListDock->addConnection(c);

    connect( c, &Connection::syncDone, this, [=]
    {
        gotEvents(c);

        // Borrowed the logic from Quiark's code in Tensor to cache not too
        // aggressively and not on the first sync. The static variable instance
        // is created per-closure, meaning per-connection (which is why this
        // code is not in gotEvents() ).
        static int counter = 0;
        if (++counter % 17 == 2)
            c->saveState();
    } );
    connect( c, &Connection::loggedOut, this, [=]
    {
        statusBar()->showMessage(tr("Logged out as %1").arg(c->userId()), 3000);
        dropConnection(c);
    });
    connect( c, &Connection::networkError, this, [=]{ networkError(c); } );
    connect( c, &Connection::loginError,
             this, [=](const QString& msg){ loginError(c, msg); } );
    connect( c, &Connection::newRoom, systemTray, &SystemTray::newRoom );
    connect( c, &Connection::aboutToDeleteRoom,
             this, [this] (QMatrixClient::Room* r)
    {
        if (currentRoom == r)
            selectRoom(nullptr);
    });

    QString accountCaption = c->userId();
    if (!deviceName.isEmpty())
        accountCaption += '/' % deviceName;
    QString menuCaption = accountCaption;
    if (connections.size() < 10)
        menuCaption.prepend('&' % QString::number(connections.size()) % ' ');
    auto accountMenu = new QMenu(menuCaption, connectionMenu);
    accountMenu->addAction(tr("Show &access token"), this, [=]
    {
        const QString aToken = c->accessToken();
        auto accountTokenBox = new QMessageBox(QMessageBox::Information,
            tr("Access token for %1").arg(accountCaption),
            tr("Your access token is %1...%2;"
               " click \"Show details...\" for the full token")
               .arg(aToken.left(5), aToken.right(5)),
            QMessageBox::Close, this, Qt::Dialog);
        accountTokenBox->setModal(false);
        accountTokenBox->setTextInteractionFlags(
                    Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);
        accountTokenBox->setDetailedText(aToken);
        accountTokenBox->setAttribute(Qt::WA_DeleteOnClose);
        accountTokenBox->show();
    });
    accountMenu->addAction(tr("&Logout"), this, [=] { logout(c); });
    auto menuAction =
            connectionMenu->insertMenu(accountListGrowthPoint, accountMenu);
    connect( c, &Connection::destroyed, connectionMenu, [this, menuAction]
    {
        connectionMenu->removeAction(menuAction);
    });

    getNewEvents(c);
}

void MainWindow::dropConnection(Connection* c)
{
    Q_ASSERT_X(c, __FUNCTION__, "Attempt to drop a null connection");

    if (currentRoom && currentRoom->connection() == c)
        selectRoom(nullptr);
    connections.removeOne(c);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
    logoutOnExit.removeOne(c);
#else
    const auto i = logoutOnExit.indexOf(c);
    if (i >= 0)
        logoutOnExit.remove(i);
#endif

    Q_ASSERT(!connections.contains(c) && !logoutOnExit.contains(c) &&
             !c->syncJob());
    c->deleteLater();
}

void MainWindow::showFirstSyncIndicator()
{
    busyLabel->show();
    busyIndicator->start();
    statusBar()->showMessage("Syncing, please wait");
}

void MainWindow::showLoginWindow(const QString& statusMessage)
{
    LoginDialog dialog(this);
    dialog.setStatusMessage(statusMessage);
    if( dialog.exec() )
    {
        auto connection = dialog.releaseConnection();
        QMatrixClient::AccountSettings account(connection->userId());
        account.setKeepLoggedIn(dialog.keepLoggedIn());
        if (dialog.keepLoggedIn())
        {
            account.setHomeserver(connection->homeserver());
            account.setDeviceId(connection->deviceId());
            account.setDeviceName(dialog.deviceName());
            account.clearAccessToken(); // Drop the legacy - just in case
            if (!saveAccessToken(account, connection->accessToken()))
                qWarning() << "Couldn't save access token";
        } else
            logoutOnExit.push_back(connection);
        account.sync();

        showFirstSyncIndicator();
        addConnection(connection, dialog.deviceName());
    }
}

void MainWindow::invokeLogin()
{
    using namespace QMatrixClient;
    SettingsGroup settings("Accounts");
    bool autoLoggedIn = false;
    for(const auto& accountId: settings.childGroups())
    {
        AccountSettings account { accountId };
        if (!account.homeserver().isEmpty())
        {
            auto accessToken = loadAccessToken(account);
            if (accessToken.isEmpty())
            {
                // Try to look in the legacy location (QSettings) and if found,
                // migrate it from there to a file.
                accessToken = account.accessToken().toLatin1();
                if (accessToken.isEmpty())
                    continue; // No access token anywhere, no autologin

                saveAccessToken(account, accessToken);
                account.clearAccessToken(); // Clean the old place
            }

            autoLoggedIn = true;
            auto c = new Connection(account.homeserver());
            auto deviceName = account.deviceName();
            connect(c, &Connection::connected, this,
                [=] {
                    c->loadState();
                    addConnection(c, deviceName);
                });
            c->connectWithToken(account.userId(), accessToken,
                                account.deviceId());
        }
    }
    if (autoLoggedIn)
        showFirstSyncIndicator();
    else
        QTimer::singleShot(0, this, SLOT(showLoginWindow()));
}

void MainWindow::loginError(Connection* c, const QString& message)
{
    Q_ASSERT_X(c, __FUNCTION__, "Login error on a null connection");
    emit c->loggedOut(); // Short circuit login error to logged-out event
    showLoginWindow(message);
}

void MainWindow::logout(Connection* c)
{
    Q_ASSERT_X(c, __FUNCTION__, "Logout on a null connection");

    QMatrixClient::AccountSettings account { c->userId() };
    QFile(accessTokenFileName(account)).remove();

    c->logout();
}

void MainWindow::selectRoom(QuaternionRoom* r)
{
    currentRoom = r;
    setWindowTitle(r ? r->displayName() : QString());
    chatRoomWidget->setRoom(r);
    userListDock->setRoom(r);
}

QMatrixClient::Connection* MainWindow::chooseConnection()
{
    Connection* connection = nullptr;
    QStringList names; names.reserve(connections.size());
    for (auto c: connections)
        names.push_back(c->userId());
    const auto choice = QInputDialog::getItem(this,
            tr("Choose the account to join the room"), "", names, -1, false);
    if (choice.isEmpty())
        return nullptr;

    for (auto c: connections)
        if (c->userId() == choice)
        {
            connection = c;
            break;
        }
    Q_ASSERT(connection);
    return connection;
}

void MainWindow::joinRoom(const QString& roomAlias, Connection* connection)
{
    if (connections.isEmpty())
    {
        QMessageBox::warning(this, tr("No connections"),
            tr("Please connect to a server before joining a room"),
            QMessageBox::Close, QMessageBox::Close);
        return;
    }

    if (!connection)
    {
        if (currentRoom && !roomAlias.isEmpty())
        {
            connection = currentRoom->connection();
            if (connections.size() > 1)
            {
                // Double check the user intention
                QMessageBox confirmBox(QMessageBox::Question,
                    tr("Joining %1 as %2").arg(roomAlias, connection->userId()),
                    tr("Join room %1 under account %2?")
                        .arg(roomAlias, connection->userId()),
                    QMessageBox::Ok|QMessageBox::Cancel, this);
                confirmBox.setButtonText(QMessageBox::Ok, tr("Join"));
                auto* chooseAccountButton =
                    confirmBox.addButton(tr("Choose account..."),
                                         QMessageBox::ActionRole);

                if (confirmBox.exec() == QMessageBox::Cancel)
                    return;
                if (confirmBox.clickedButton() == chooseAccountButton)
                    connection = chooseConnection();
            }
        } else
            connection = connections.size() == 1 ? connections.front() :
                         chooseConnection();
    }
    if (!connection)
        return; // No default connection and the user discarded the dialog

    QString room = roomAlias;
    while (room.isEmpty())
    {
        QInputDialog roomInput (this);
        roomInput.setWindowTitle(tr("Enter room id or alias to join"));
        roomInput.setLabelText(
            tr("Enter an id or alias of the room. You will join as %1")
                .arg(connection->userId()));
        roomInput.setOkButtonText(tr("Join"));
        roomInput.setSizeGripEnabled(true);
        // TODO: Provide a button to select the joining account
        if (roomInput.exec() == QDialog::Rejected)
            return;
        // TODO: Check validity, not only non-emptyness
        if (!roomInput.textValue().isEmpty())
            room = roomInput.textValue();
        else
            QMessageBox::warning(this, tr("No room id or alias specified"),
                                 tr("Please specify non-empty id or alias"),
                                 QMessageBox::Close, QMessageBox::Close);
    }

    using QMatrixClient::JoinRoomJob;
    auto job = connection->joinRoom(room);
    // Connection::joinRoom() already connected to success() the code that
    // initialises the room in the library, which in turn causes RoomListModel
    // to update the room list. So the below connection to success() will be
    // triggered after all the initialisation have happened.
    connect(job, &JoinRoomJob::success, this, [=]
    {
        statusBar()->showMessage(tr("Joined %1 as %2")
                                 .arg(roomAlias, connection->userId()));
    });
    connect(job, &JoinRoomJob::failure, this, [=] {
        QMessageBox messageBox(QMessageBox::Warning,
                               tr("Failed to join room"),
                               tr("Joining request returned an error"),
                               QMessageBox::Close, this);
        messageBox.setWindowModality(Qt::WindowModal);
        messageBox.setTextFormat(Qt::PlainText);
        messageBox.setDetailedText(job->errorString());
        switch (job->error()) {
            case JoinRoomJob::NotFoundError:
                messageBox.setText(
                    tr("Room %1 not found on the server").arg(roomAlias));
                break;
            case JoinRoomJob::IncorrectRequestError:
                messageBox.setText(
                    tr("Incorrect id or alias: %1").arg(roomAlias));
                break;
            default:
                messageBox.setText(tr("Error joining %1: %2"));
        }
        messageBox.exec();
    });
}

void MainWindow::getNewEvents(Connection* c)
{
    Q_ASSERT_X(c, __FUNCTION__, "Attempt to sync on null connection");
    c->sync(30*1000);
}

void MainWindow::gotEvents(Connection* c)
{
    Q_ASSERT_X(c, __FUNCTION__, "Null connection");
    if( busyLabel->isVisible() )
    {
        busyLabel->hide();
        busyIndicator->stop();
        statusBar()->showMessage(tr("Sync completed - have a good chat"), 3000);
    }
    getNewEvents(c);
}

void MainWindow::showMillisToRecon(Connection* c)
{
    // TODO: when there are several connections and they are failing, these
    // notifications render a mess, fighting for the same status bar. Either
    // switch to a set of icons in the status bar or find a stacking
    // notifications engine already instead of the status bar.
    statusBar()->showMessage(
        tr("Couldn't connect to the server as %1; will retry within %2 seconds")
                .arg(c->userId()).arg((c->millisToReconnect() + 999) / 1000)); // Integer ceiling
}

void MainWindow::networkError(Connection* c)
{
    Q_ASSERT_X(c, __FUNCTION__, "Network error on a null connection");
    auto timer = new QTimer(this);
    timer->start(1000);
    showMillisToRecon(c);
    timer->connect(timer, &QTimer::timeout, [=] {
        if (c->millisToReconnect() > 0)
            showMillisToRecon(c);
        else
        {
            statusBar()->showMessage(tr("Reconnecting..."), 5000);
            timer->deleteLater();
        }
    });
}

void MainWindow::sslErrors(QNetworkReply* reply, const QList<QSslError>& errors)
{
    for (const auto& error: errors)
    {
        QMessageBox msgBox(QMessageBox::Warning, tr("SSL error"),
                           error.errorString(),
                           QMessageBox::Abort|QMessageBox::Ignore, this);
        if (!error.certificate().isNull())
            msgBox.setDetailedText(error.certificate().toText());
        if (msgBox.exec() == QMessageBox::Abort)
            return;
        NetworkAccessManager::instance()->addIgnoredSslError(error);
    }
    reply->ignoreSslErrors(errors);
}

void MainWindow::proxyAuthenticationRequired(const QNetworkProxy&,
                                             QAuthenticator* auth)
{
    Dialog authDialog(tr("Proxy needs authentication"), this);
    auto layout = new QFormLayout;
    auto userEdit = new QLineEdit;
    layout->addRow(tr("User name"), userEdit);
    auto pwdEdit = new QLineEdit;
    pwdEdit->setEchoMode(QLineEdit::Password);
    layout->addRow(tr("Password"), pwdEdit);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok|
                                        QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Connect"));
    connect(buttons, &QDialogButtonBox::accepted, &authDialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &authDialog, &QDialog::reject);
    layout->addWidget(buttons);
    authDialog.setLayout(layout);
    if (authDialog.exec() == QDialog::Accepted)
    {
        auth->setUser(userEdit->text());
        auth->setPassword(pwdEdit->text());
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    for (auto c: connections)
    {
        c->saveState();
        c->stopSync(); // Instead of deleting the connection, merely stop it
//        dropConnection(c);
    }
    for (auto c: logoutOnExit)
        c->logout(); // For the record, dropConnection() does it automatically
    saveSettings();
    event->accept();
}

