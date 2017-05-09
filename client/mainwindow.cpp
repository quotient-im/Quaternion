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
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QAction>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QLabel>
#include <QtGui/QMovie>
#include <QtGui/QCloseEvent>

#include "jobs/joinroomjob.h"
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
    connect( chatRoomWidget, &ChatRoomWidget::joinCommandEntered, this, &MainWindow::joinRoom );
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

ChatRoomWidget* MainWindow::getChatRoomWidget() const
{
   return chatRoomWidget;
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

    // View menu
    auto viewMenu = menuBar()->addMenu(tr("&View"));

    viewMenu->addAction(roomListDock->toggleViewAction());
    viewMenu->addAction(userListDock->toggleViewAction());

    // Room menu
    auto roomMenu = menuBar()->addMenu(tr("&Room"));

    auto joinRoomAction = roomMenu->addAction(tr("&Join Room..."));
    connect( joinRoomAction, &QAction::triggered, this, [=]{ joinRoom(); } );
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

        connection->stopSync();
        connection->deleteLater();
    }

    connection = newConnection;
    loginAction->setEnabled(connection == nullptr); // Don't support multiple accounts yet
    logoutAction->setEnabled(connection != nullptr);

    if (connection)
    {
        roomListDock->setConnection(connection);

        using QMatrixClient::Connection;
        connect( connection, &Connection::networkError, this, &MainWindow::networkError );
        connect( connection, &Connection::syncDone, this, &MainWindow::gotEvents );
        connect( connection, &Connection::connected, this, &MainWindow::initialSync );
        // Short-circuit login errors to logged-out events
        connect( connection, &Connection::loginError, connection, &Connection::loggedOut );
        connect( connection, &Connection::loggedOut, [=]{ loggedOut(); } );
        connect( connection, &Connection::newRoom, systemTray, &SystemTray::newRoom );
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

void MainWindow::joinRoom(const QString& roomAlias)
{
    if (!connection)
    {
        QMessageBox::warning(this, tr("No connection"),
            tr("Please connect to a server before joining a room"),
            QMessageBox::Close, QMessageBox::Close);
        return;
    }

    QString room = roomAlias;
    if (room.isEmpty())
        room = QInputDialog::getText(this, tr("Enter room alias to join"),
                tr("Enter an alias of the room, including the server part"));

    // Dialog rejected, nothing to do.
    if (room.isEmpty())
        return;

    using QMatrixClient::JoinRoomJob;
    auto job = connection->joinRoom(room);
    connect(job, &QMatrixClient::BaseJob::failure, this, [=] {
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
                    tr("Room with id or alias %1 does not seem to exist").arg(roomAlias));
                break;
            case JoinRoomJob::IncorrectRequestError:
                messageBox.setText(
                    tr("Incorrect id or alias: %1").arg(roomAlias));
                break;
            default:;
        }
        messageBox.exec();
    });
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

void showMillisToRecon(QStatusBar* statusBar, int millis)
{
    statusBar->showMessage(
        MainWindow::tr("Network error when syncing; will retry within %1 seconds")
        .arg((millis + 999) / 1000)); // Integer ceiling
}

void MainWindow::networkError()
{
    auto timer = new QTimer(this);
    timer->start(std::min(1000, connection->millisToReconnect()));
    showMillisToRecon(statusBar(), timer->remainingTime());
    timer->connect(timer, &QTimer::timeout, [=] {
        if (connection->millisToReconnect() > 0)
            showMillisToRecon(statusBar(), connection->millisToReconnect());
        else
        {
            statusBar()->showMessage(tr("Reconnecting..."), 5000);
            timer->deleteLater();
        }
    });
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    setConnection(nullptr);
    saveSettings();
    event->accept();
}
