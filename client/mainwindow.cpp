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

#include <QtCore/QTimer>
#include <QtCore/QDebug>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMenu>
#include <QtWidgets/QAction>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QLabel>
#include <QtGui/QMovie>

#include "quaternionconnection.h"
#include "quaternionroom.h"
#include "roomlistdock.h"
#include "userlistdock.h"
#include "chatroomwidget.h"
#include "logindialog.h"
#include "systemtray.h"
#include "settings.h"

MainWindow::MainWindow()
{
    setWindowIcon(QIcon(":/icon.png"));
    connection = nullptr;
    roomListDock = new RoomListDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, roomListDock);
    userListDock = new UserListDock(this);
    addDockWidget(Qt::RightDockWidgetArea, userListDock);
    chatRoomWidget = new ChatRoomWidget(this);
    setCentralWidget(chatRoomWidget);
    connect( chatRoomWidget, &ChatRoomWidget::joinRoomNeedsInteraction, this, &MainWindow::showJoinRoomDialog);
    connect( roomListDock, &RoomListDock::roomSelected, chatRoomWidget, &ChatRoomWidget::setRoom );
    connect( roomListDock, &RoomListDock::roomSelected, userListDock, &UserListDock::setRoom );
    connect( chatRoomWidget, &ChatRoomWidget::showStatusMessage, statusBar(), &QStatusBar::showMessage );
    systemTray = new SystemTray(this);
    createMenu();
    loadSettings();
    statusBar(); // Make sure it is displayed from the start
    show();
    systemTray->show();
    QTimer::singleShot(0, this, SLOT(initialize()));
}

MainWindow::~MainWindow()
{
}

void MainWindow::createMenu()
{
    // Connection menu
    auto connectionMenu = menuBar()->addMenu(tr("&Connection"));

    loginAction = connectionMenu->addAction(tr("&Login..."));
    connect( loginAction, &QAction::triggered, [=]{ showLoginWindow(); } );

    logoutAction = connectionMenu->addAction(tr("&Logout"));
    connect( logoutAction, &QAction::triggered, [=]{ logout(); } );
    logoutAction->setEnabled(false); // we start in a logged out state

    connectionMenu->addSeparator();

    auto quitAction = connectionMenu->addAction(tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect( quitAction, &QAction::triggered, qApp, &QApplication::closeAllWindows );

    // Room menu
    auto roomMenu = menuBar()->addMenu(tr("&Room"));

    auto joinRoomAction = roomMenu->addAction(tr("&Join Room..."));
    connect( joinRoomAction, &QAction::triggered, [=]{ showJoinRoomDialog(); } );
}

void MainWindow::loadSettings()
{
    QMatrixClient::SettingsGroup sg("UI/MainWindow");
    if (sg.contains("normal_geometry"))
        setGeometry(sg.value("normal_geometry").toRect());
    if (sg.value("maximized").toBool())
        showMaximized();
    if (sg.contains("parts_state"))
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

void MainWindow::enableDebug()
{
    chatRoomWidget->enableDebug();
}

void MainWindow::initialize()
{
    busyIndicator = new QMovie(":/busy.gif");
    busyLabel = new QLabel(this);
    busyLabel->setMovie(busyIndicator);
    busyLabel->hide();
    statusBar()->setSizeGripEnabled(false);
    statusBar()->addPermanentWidget(busyLabel);

    invokeLogin();
}

void MainWindow::setConnection(QuaternionConnection* newConnection)
{
    if (connection)
    {
        chatRoomWidget->setConnection(nullptr);
        userListDock->setConnection(nullptr);
        roomListDock->setConnection(nullptr);
        systemTray->setConnection(nullptr);

        connection->disconnectFromServer();
        connection->disconnect(); // Disconnect everybody from all connection's signals
        connection->deleteLater();
    }

    connection = newConnection;
    loginAction->setEnabled(connection == nullptr); // Don't support multiple accounts yet
    logoutAction->setEnabled(connection != nullptr);

    if (connection)
    {
        chatRoomWidget->setConnection(connection);
        userListDock->setConnection(connection);
        roomListDock->setConnection(connection);
        systemTray->setConnection(connection);

        using QMatrixClient::Connection;
        connect( connection, &Connection::connectionError, this, &MainWindow::connectionError );
        connect( connection, &Connection::syncDone, this, &MainWindow::gotEvents );
        connect( connection, &Connection::connected, this, &MainWindow::initialSync );
        connect( connection, &Connection::reconnected, this, &MainWindow::getNewEvents );
        connect( connection, &Connection::loginError, this, &MainWindow::loggedOut );
        connect( connection, &Connection::loggedOut, [=]{ loggedOut(); } );
    }
}

void MainWindow::showLoginWindow(const QString& statusMessage)
{
    LoginDialog dialog(this);
    dialog.setStatusMessage(statusMessage);
    if( dialog.exec() )
    {
        setConnection(dialog.connection());

        QMatrixClient::AccountSettings account(connection->userId());
        account.setKeepLoggedIn(dialog.keepLoggedIn());
        if (dialog.keepLoggedIn())
        {
            account.setHomeserver(connection->homeserver());
            account.setAccessToken(connection->accessToken());
        }
        account.sync();

        initialSync();
    }
}

void MainWindow::invokeLogin()
{
    using namespace QMatrixClient;
    SettingsGroup settings("Accounts");
    for(auto accountId: settings.childGroups())
    {
        AccountSettings account { accountId };
        if (!account.accessToken().isEmpty())
        {
            // TODO: Support multiple accounts
            // Right now the code just finds the first configured account,
            // rather than connects all available.
            setConnection(new QuaternionConnection(account.homeserver()));
            connection->connectWithToken(account.userId(), account.accessToken());
            return;
        }
    }
    // No accounts to automatically log into
    showLoginWindow();
}

void MainWindow::logout()
{
    if( !connection )
        return; // already logged out

    QMatrixClient::AccountSettings account { connection->userId() };
    account.clearAccessToken();
    account.sync();

    connection->logout();
}

void MainWindow::loggedOut(const QString& message)
{
    loginAction->setEnabled(true);
    logoutAction->setEnabled(false);
    setConnection(nullptr);
    showLoginWindow(message);
}

void MainWindow::initialSync()
{
    setWindowTitle(connection->userId());
    busyLabel->show();
    busyIndicator->start();
    statusBar()->showMessage("Syncing, please wait...");
    getNewEvents();
}

void MainWindow::getNewEvents()
{
    connection->sync(30*1000);
}

void MainWindow::gotEvents()
{
    if( busyLabel->isVisible() )
    {
        busyLabel->hide();
        busyIndicator->stop();
        statusBar()->showMessage(tr("Connected as %1").arg(connection->userId()), 5000);
    }
    getNewEvents();
}

void MainWindow::connectionError(QString error)
{
    qDebug() << error;
    qDebug() << "reconnecting...";
    connection->reconnect();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    setConnection(nullptr);
    saveSettings();
    event->accept();
}

void MainWindow::showJoinRoomDialog()
{
    bool ok;
    QString room = QInputDialog::getText(this, tr("Join Room"), tr("Enter the name of the room"),
                                         QLineEdit::Normal, QString(), &ok);
    if( ok && !room.isEmpty() )
    {
        connection->joinRoom(room);
    }
}


