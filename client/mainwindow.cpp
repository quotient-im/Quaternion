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
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QLabel>
#include <QtGui/QMovie>
#include <QtGui/QCloseEvent>

#include "jobs/joinroomjob.h"
#include "lib/connection.h"
#include "roomlistdock.h"
#include "userlistdock.h"
#include "chatroomwidget.h"
#include "logindialog.h"
#include "systemtray.h"
#include "settings.h"

class QuaternionRoom;

MainWindow::MainWindow()
{
    Connection::setRoomType<QuaternionRoom>();
    setWindowIcon(QIcon(":/icon.png"));
    loadSettings();

    roomListDock = new RoomListDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, roomListDock);
    userListDock = new UserListDock(this);
    addDockWidget(Qt::RightDockWidgetArea, userListDock);
    chatRoomWidget = new ChatRoomWidget(this);
    setCentralWidget(chatRoomWidget);
    connect( chatRoomWidget, &ChatRoomWidget::joinCommandEntered, this, &MainWindow::joinRoom );
    connect( roomListDock, &RoomListDock::roomSelected, [=](QuaternionRoom *r)
    {
        setWindowTitle(r->displayName());
        chatRoomWidget->setRoom(r);
        userListDock->setRoom(r);
    } );
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

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
    connectionMenu->addAction(tr("&Login..."), this, [=]{ showLoginWindow(); } );
#else
    connectionMenu->addAction(tr("&Login..."), this, SLOT(showLoginWindow()));
#endif

    connectionMenu->addSeparator();
    // Logout actions will be added between these two separators - see addConnection()
    accountListGrowthPoint = connectionMenu->addSeparator();

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
    connect( c, &Connection::loggedOut, this, [=]{ dropConnection(c); } );
    connect( c, &Connection::networkError, this, [=]{ networkError(c); } );
    connect( c, &Connection::loginError,
             this, [=](const QString& msg){ loginError(c, msg); } );
    connect( c, &Connection::newRoom, systemTray, &SystemTray::newRoom );

    const QString logoutCaption = deviceName.isEmpty() ?
                tr("Logout %1").arg(c->userId()) :
                tr("Logout %1/%2").arg(c->userId(), deviceName);
    const auto logoutAction = new QAction(logoutCaption, c);
    connectionMenu->insertAction(accountListGrowthPoint, logoutAction);
    connect( logoutAction, &QAction::triggered, this, [=]{ logout(c); } );
    connect( c, &Connection::destroyed, this, [=]
    {
        connectionMenu->removeAction(logoutAction);
    } );

    getNewEvents(c);
}

void MainWindow::dropConnection(Connection* c)
{
    Q_ASSERT_X(c, __FUNCTION__, "Attempt to drop a null connection");

    connections.removeOne(c);
    Q_ASSERT(!connections.contains(c));
    c->deleteLater();
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
            // FIXME: #181
            account.setAccessToken(connection->accessToken());
            account.setDeviceId(connection->deviceId());
            account.setDeviceName(dialog.deviceName());
        }
        account.sync();

        addConnection(connection, dialog.deviceName());
    }
}

void MainWindow::invokeLogin()
{
    using namespace QMatrixClient;
    SettingsGroup settings("Accounts");
    for(const auto& accountId: settings.childGroups())
    {
        AccountSettings account { accountId };
        if (!account.accessToken().isEmpty())
        {
            auto c = new Connection(account.homeserver());
            c->connectWithToken(account.userId(), account.accessToken(),
                                account.deviceId());
            c->loadState();
            addConnection(c, account.deviceName());
        }
    }
    if (connections.isEmpty())
        QTimer::singleShot(0, this, SLOT(showLoginWindow()));
    else
    {
        busyLabel->show();
        busyIndicator->start();
        statusBar()->showMessage("Syncing, please wait...");
    }
}

void MainWindow::loginError(Connection* c, const QString& message)
{
    Q_ASSERT_X(c, __FUNCTION__, "Login error on a null connection");
    // FIXME: Make ConnectionManager instead of such hacks
    emit c->loggedOut(); // Short circuit login error to logged-out event
    showLoginWindow(message);
}

void MainWindow::logout(Connection* c)
{
    Q_ASSERT_X(c, __FUNCTION__, "Logout on a null connection");

    QMatrixClient::AccountSettings account { c->userId() };
    account.clearAccessToken();
    account.sync();

    c->logout();
}

void MainWindow::joinRoom(const QString& roomAlias)
{
    if (connections.isEmpty())
    {
        QMessageBox::warning(this, tr("No connections"),
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
    // FIXME: only the first account is used to join a room
    auto job = connections.front()->joinRoom(room);
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
        statusBar()->showMessage(tr("Connected as %1").arg(c->userId()), 5000);
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

void MainWindow::closeEvent(QCloseEvent* event)
{
    for (auto c: connections)
    {
        c->saveState();
        dropConnection(c);
    }
    saveSettings();
    event->accept();
}

