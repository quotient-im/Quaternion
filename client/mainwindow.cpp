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
#include "roomdialogs.h"
#include "systemtrayicon.h"

#include <csapi/joining.h>
#include <connection.h>
#include <networkaccessmanager.h>
#include <settings.h>
#include <logging.h>
#include <user.h>

#include <QtCore/QTimer>
#include <QtCore/QDebug>
#include <QtCore/QStandardPaths>
#include <QtCore/QStringBuilder>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>
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
#include <QtGui/QPixmap>
#include <QtGui/QCloseEvent>
#include <QtGui/QDesktopServices>

using QMatrixClient::NetworkAccessManager;
using QMatrixClient::AccountSettings;

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
    connect( userListDock, &UserListDock::userMentionRequested,
             chatRoomWidget, &ChatRoomWidget::insertMention);

    createMenu();
    systemTrayIcon = new SystemTrayIcon(this);
    systemTrayIcon->show();

    busyIndicator = new QMovie(":/busy.gif");
    busyLabel = new QLabel(this);
    busyLabel->setMovie(busyIndicator);
    statusBar()->setSizeGripEnabled(false);
    statusBar()->addPermanentWidget(busyLabel);
    statusBar()->showMessage(tr("Loading..."));
    loadSettings(); // Only GUI, account settings will be loaded in invokeLogin

    busyLabel->show();
    busyIndicator->start();
    QTimer::singleShot(0, this, SLOT(invokeLogin()));
}

ChatRoomWidget* MainWindow::getChatRoomWidget() const
{
   return chatRoomWidget;
}

template <typename DialogT, typename... DialogArgTs>
void summon(QPointer<DialogT>& dlg, DialogArgTs&&... dialogArgs)
{
    if (!dlg)
    {
        dlg = new DialogT(std::forward<DialogArgTs>(dialogArgs)...);
        dlg->setModal(false);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
    }

    dlg->reactivate();
}

QAction* MainWindow::addTimelineOptionCheckbox(QMenu* parent,
    const QString& text, const QString& statusTip, const QString& settingsKey,
    bool defaultValue)
{
    using QMatrixClient::SettingsGroup;
    auto action =
        parent->addAction(text,
            [this,settingsKey] (bool checked)
            {
                SettingsGroup("UI").setValue(settingsKey, checked);
                chatRoomWidget->setRoom(nullptr);
                chatRoomWidget->setRoom(currentRoom);
            });
    action->setStatusTip(statusTip);
    action->setCheckable(true);
    action->setChecked(
        SettingsGroup("UI").value(settingsKey, defaultValue).toBool());
    return action;
}

void MainWindow::createMenu()
{
    using QMatrixClient::Settings;

    // Connection menu
    connectionMenu = menuBar()->addMenu(tr("&Accounts"));

    connectionMenu->addAction(QIcon::fromTheme("im-user"), tr("&Login..."),
        this, [=]{ showLoginWindow(); } );

    connectionMenu->addSeparator();
    // Account submenus will be added in this place - see addConnection()
    accountListGrowthPoint = connectionMenu->addSeparator();

    connectionMenu->addAction(QIcon::fromTheme("application-exit"),
        tr("&Quit"), qApp, &QApplication::closeAllWindows, QKeySequence::Quit);

    // View menu
    auto viewMenu = menuBar()->addMenu(tr("&View"));

    auto dockPanesMenu = viewMenu->addMenu(
        QIcon::fromTheme("labplot-editvlayout"), tr("Dock &panels"));
    roomListDock->toggleViewAction()
            ->setStatusTip("Show/hide Rooms dock panel");
    dockPanesMenu->addAction(roomListDock->toggleViewAction());
    userListDock->toggleViewAction()
        ->setStatusTip("Show/hide Users dock panel");
    dockPanesMenu->addAction(userListDock->toggleViewAction());

    viewMenu->addSeparator();

    auto showEventsMenu = viewMenu->addMenu(tr("&Display in timeline"));
    addTimelineOptionCheckbox(
        showEventsMenu,
        tr("Normal &join/leave events"),
        tr("Show join and leave events that don't couple in a no-change pair"),
        QStringLiteral("show_joinleave"),
        true
    );
    addTimelineOptionCheckbox(
        showEventsMenu,
        tr("&Redacted events"),
        tr("Show redacted events in the timeline as 'Redacted'"
           " instead of hiding them entirely"),
        QStringLiteral("show_redacted")
    );
    addTimelineOptionCheckbox(
        showEventsMenu,
        tr("&No-effect activity"),
        tr("Show join and leave events that couple in a no-change pair,"
           " possibly with redactions between"),
        QStringLiteral("show_spammy")
    );

    viewMenu->addSeparator();

    viewMenu->addAction(tr("Edit tags order"), [this]
    {
        static const auto SettingsKey = QStringLiteral("tags_order");
        QMatrixClient::SettingsGroup sg { QStringLiteral("UI/RoomsDock") };
        const auto savedOrder = sg.get<QStringList>(SettingsKey).join('\n');
        bool ok;
        const auto newOrder = QInputDialog::getMultiLineText(this,
                tr("Edit tags order"),
                tr("Tags can be wildcarded by * next to dot(s)\n"
                   "Clear the box to reset to defaults\n"
                   "org.qmatrixclient. tags: invite, left, direct, none"),
                savedOrder, &ok);
        if (ok)
        {
            if (newOrder.isEmpty())
                sg.remove(SettingsKey);
            else if (newOrder != savedOrder)
                sg.setValue(SettingsKey, newOrder.split('\n'));
            roomListDock->updateSortingMode();
        }
    });

    // Room menu
    auto roomMenu = menuBar()->addMenu(tr("&Room"));

    roomSettingsAction =
        roomMenu->addAction(QIcon::fromTheme("user-group-properties"),
        tr("Change room &settings..."), [this]
        {
            static QHash<QuaternionRoom*, QPointer<RoomSettingsDialog>> dlgs;
            summon(dlgs[currentRoom], currentRoom, this);
        });
    roomSettingsAction->setDisabled(true);
    roomMenu->addSeparator();
    createRoomAction =
        roomMenu->addAction(QIcon::fromTheme("user-group-new"),
        tr("Create &new room..."), [this]
        {
            static QPointer<CreateRoomDialog> dlg;
            summon(dlg, connections, this);
        });
    createRoomAction->setDisabled(true);
    roomMenu->addAction(QIcon::fromTheme("list-add-user"),
        tr("&Direct chat..."), [=]{ directChat(); });
    roomMenu->addAction(QIcon::fromTheme("list-add"), tr("&Join room..."),
        [=]{ joinRoom(); } );
    roomMenu->addSeparator();
    roomMenu->addAction(QIcon::fromTheme("window-close"),
        tr("&Close current room"), [this] { selectRoom(nullptr); },
        QKeySequence::Close);

    // Settings menu
    auto settingsMenu = menuBar()->addMenu(tr("&Settings"));

    // Help menu
    auto helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(QIcon::fromTheme("help-about"), tr("&About"),
        [=]{ showAboutWindow(); });

    {
        auto notifGroup = new QActionGroup(this);
        connect(notifGroup, &QActionGroup::triggered, this,
            [] (QAction* notifAction)
            {
                notifAction->setChecked(true);
                Settings().setValue("UI/notifications",
                                    notifAction->data().toString());
            });

        auto noNotif = notifGroup->addAction(tr("&Highlight only"));
        noNotif->setData(QStringLiteral("none"));
        noNotif->setStatusTip(tr("Notifications are entirely suppressed"));
        auto gentleNotif = notifGroup->addAction(tr("&Non-intrusive"));
        gentleNotif->setData(QStringLiteral("non-intrusive"));
        gentleNotif->setStatusTip(
            tr("Show notifications but do not activate the window"));
        auto fullNotif = notifGroup->addAction(tr("&Full"));
        fullNotif->setData(QStringLiteral("intrusive"));
        fullNotif->setStatusTip(
            tr("Show notifications and activate the window"));

        auto notifMenu = settingsMenu->addMenu(
            QIcon::fromTheme("preferences-desktop-notification"),
            tr("Notifications"));
        for (auto a: {noNotif, gentleNotif, fullNotif})
        {
            a->setCheckable(true);
            notifMenu->addAction(a);
        }

        const auto curSetting = Settings().value("UI/notifications",
                                                 fullNotif->data().toString());
        if (curSetting == noNotif->data().toString())
            noNotif->setChecked(true);
        else if (curSetting == gentleNotif->data().toString())
            gentleNotif->setChecked(true);
        else
            fullNotif->setChecked(true);
    }
    {
        auto layoutGroup = new QActionGroup(this);
        connect(layoutGroup, &QActionGroup::triggered, this,
            [this] (QAction* action)
        {
            action->setChecked(true);
            Settings().setValue("UI/timeline_style", action->data().toString());
            chatRoomWidget->setRoom(nullptr);
            chatRoomWidget->setRoom(currentRoom);
        });

        auto defaultLayout = layoutGroup->addAction(tr("Default"));
        defaultLayout->setStatusTip(
            tr("The layout with author labels above blocks of messages"));
        auto xchatLayout = layoutGroup->addAction(tr("XChat"));
        xchatLayout->setData(QStringLiteral("xchat"));
        xchatLayout->setStatusTip(
            tr("The layout with author labels to the left from each message"));

        auto layoutMenu = settingsMenu->addMenu(QIcon::fromTheme("table"),
            tr("Timeline layout"));
        for (auto a: {defaultLayout, xchatLayout})
        {
            a->setCheckable(true);
            layoutMenu->addAction(a);
        }

        const auto curSetting = Settings().value("UI/timeline_style",
                                                 defaultLayout->data().toString());
        if (curSetting == xchatLayout->data().toString())
            xchatLayout->setChecked(true);
        else
            defaultLayout->setChecked(true);
    }
    addTimelineOptionCheckbox(
        settingsMenu,
        tr("Load full-size images at once"),
        tr("Automatically download a full-size image instead of a thumbnail"),
        QStringLiteral("autoload_images"), true
    );

    settingsMenu->addSeparator();
    settingsMenu->addAction(QIcon::fromTheme("preferences-system-network"),
        tr("Configure &network proxy..."), [this]
    {
        static QPointer<NetworkConfigDialog> dlg;
        summon(dlg, this);
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

inline QString accessTokenFileName(const AccountSettings& account)
{
    QString fileName = account.userId();
    fileName.replace(':', '_');
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
            % '/' % fileName;
}

QByteArray MainWindow::loadAccessToken(const AccountSettings& account)
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

bool MainWindow::saveAccessToken(const AccountSettings& account,
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

    using Room = QMatrixClient::Room;

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
    connect( c, &Connection::syncError, this,
        [this,c] (const QString& message, const QByteArray& details) {
            QMessageBox msgBox(QMessageBox::Warning, tr("Sync failed"),
                connections.size() > 1
                    ? tr("The last sync of account %1 has failed with error: %2")
                        .arg(c->userId(), message)
                    : tr("The last sync has failed with error: %1").arg(message),
                QMessageBox::Retry|QMessageBox::Cancel, this);
            msgBox.setTextFormat(Qt::PlainText);
            msgBox.setDefaultButton(QMessageBox::Retry);
            msgBox.setInformativeText(tr(
                "Clicking 'Retry' will attempt to resume synchronisation;\n"
                "Clicking 'Cancel' will stop further synchronisation of this "
                "account until logout or Quaternion restart."));
            msgBox.setDetailedText(details);
            if (msgBox.exec() == QMessageBox::Retry)
                getNewEvents(c);
        });
    using namespace QMatrixClient;
    connect( c, &Connection::requestFailed, this,
        [this] (BaseJob* job)
        {
            if (job->isBackground())
                return;
            auto message = job->error() == BaseJob::UserConsentRequiredError
                ? tr("Before this server can process your information, you have"
                     " to agree with its terms and conditions; please click the"
                     " button below to open the web page where you can do that")
                : prettyPrint(job->errorString());

            QMessageBox msgBox(QMessageBox::Warning, job->statusCaption(),
                message, QMessageBox::Close, this);
            msgBox.setTextFormat(Qt::RichText);
            msgBox.setDetailedText(
                "Request URL: " + job->requestUrl().toDisplayString() +
                "\nResponse:\n" + job->rawData(65535));
            QPushButton* openUrlButton = nullptr;
            if (job->errorUrl().isEmpty())
                msgBox.setDefaultButton(QMessageBox::Close);
            else
            {
                openUrlButton =
                    msgBox.addButton(tr("Open web page"), QMessageBox::ActionRole);
                openUrlButton->setDefault(true);
            }
            msgBox.exec();
            if (msgBox.clickedButton() == openUrlButton)
                QDesktopServices::openUrl(job->errorUrl());
        });
    connect( c, &Connection::loginError,
             this, [=](const QString& msg){ loginError(c, msg); } );
    connect( c, &Connection::newRoom, systemTrayIcon, &SystemTrayIcon::newRoom );
    connect( c, &Connection::createdRoom, this, &MainWindow::selectRoom);
    connect( c, &Connection::joinedRoom, this, [this] (Room* r, Room* prev)
        {
            if (currentRoom == prev)
                selectRoom(r);
        });
    connect( c, &Connection::directChatAvailable, this,
             [this] (Room* r) {
                 selectRoom(r);
                 statusBar()->showMessage("Direct chat opened", 2000);
             });
    connect( c, &Connection::aboutToDeleteRoom, this,
             [this] (Room* r) {
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
    accountMenu->addAction(QIcon::fromTheme("view-certificate"),
        tr("Show &access token"), this, [=]
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
    accountMenu->addAction(QIcon::fromTheme("system-log-out"), tr("&Logout"),
                           this, [=] { logout(c); });
    auto menuAction =
            connectionMenu->insertMenu(accountListGrowthPoint, accountMenu);
    connect( c, &Connection::destroyed, connectionMenu, [this, menuAction]
    {
        connectionMenu->removeAction(menuAction);
    });
    createRoomAction->setEnabled(true);

    getNewEvents(c);
}

void MainWindow::dropConnection(Connection* c)
{
    Q_ASSERT_X(c, __FUNCTION__, "Attempt to drop a null connection");

    if (currentRoom && currentRoom->connection() == c)
        selectRoom(nullptr);
    connections.removeOne(c);
    logoutOnExit.removeOne(c);
    createRoomAction->setDisabled(connections.isEmpty());

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
        AccountSettings account(connection->userId());
        account.setKeepLoggedIn(dialog.keepLoggedIn());
        account.clearAccessToken(); // Drop the legacy - just in case
        if (dialog.keepLoggedIn())
        {
            account.setHomeserver(connection->homeserver());
            account.setDeviceId(connection->deviceId());
            account.setDeviceName(dialog.deviceName());
            if (!saveAccessToken(account, connection->accessToken()))
                qWarning() << "Couldn't save access token";
        } else
            logoutOnExit.push_back(connection);
        account.sync();

        showFirstSyncIndicator();
        addConnection(connection, dialog.deviceName());
    }
}

void MainWindow::showAboutWindow()
{
    Dialog aboutDialog(tr("About Quaternion"), QDialogButtonBox::Close, this,
                      Dialog::NoStatusLine);
    auto* layout = aboutDialog.addLayout<QVBoxLayout>();

    QLabel* imageLabel = new QLabel();
    imageLabel->setPixmap(QPixmap(":/icon.png"));
    imageLabel->setAlignment(Qt::AlignHCenter);
    layout->addWidget(imageLabel);

    auto* labelString =
            new QLabel("<h1>" + QApplication::applicationDisplayName() + " v" +
                       QApplication::applicationVersion() + "</h1>");
    labelString->setAlignment(Qt::AlignHCenter);
    layout->addWidget(labelString);

    auto* linkLabel = new QLabel(tr("<a href=\"https://matrix.org/docs/projects/client/quaternion.html\">Website</a>"));
    linkLabel->setAlignment(Qt::AlignHCenter);
    layout->addWidget(linkLabel);

    layout->addWidget(
                new QLabel(tr("Copyright (C) 2018 QMatrixClient project.")));

#ifdef GIT_SHA1
    layout->addWidget(new QLabel(tr("Built from Git, commit SHA:\n") +
                                 QStringLiteral(GIT_SHA1)));
#endif

#ifdef LIB_GIT_SHA1
    layout->addWidget(new QLabel(tr("Library commit SHA:\n") +
                                 QStringLiteral(LIB_GIT_SHA1)));
#endif

    aboutDialog.exec();
}

void MainWindow::invokeLogin()
{
    using namespace QMatrixClient;
    const auto accounts = SettingsGroup("Accounts").childGroups();
    bool autoLoggedIn = false;
    for(const auto& accountId: accounts)
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
        showLoginWindow(tr("Welcome to Quaternion"));
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

    QFile(accessTokenFileName(AccountSettings(c->userId()))).remove();

    c->logout();
}

void MainWindow::selectRoom(QMatrixClient::Room* r)
{
    QElapsedTimer et; et.start();
    currentRoom = static_cast<QuaternionRoom*>(r);
    setWindowTitle(r ? r->displayName() : QString());
    chatRoomWidget->setRoom(currentRoom);
    userListDock->setRoom(currentRoom);
    roomSettingsAction->setEnabled(r != nullptr);
    if (r && !isActiveWindow())
    {
        show();
        activateWindow();
    }
    qDebug().noquote() << et << "to "
        << (r ? "select room " + r->canonicalAlias() : "close the room");
}

QMatrixClient::Connection* MainWindow::chooseConnection()
{
    Connection* connection = nullptr;
    QStringList names; names.reserve(connections.size());
    for (auto c: qAsConst(connections))
        names.push_back(c->userId());
    const auto choice = QInputDialog::getItem(this,
            tr("Choose the account to join the room"), "", names, -1, false);
    if (choice.isEmpty())
        return nullptr;

    for (auto c: qAsConst(connections))
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

    using QMatrixClient::BaseJob;
    auto* job = connection->joinRoom(room);
    // Connection::joinRoom() already connected to success() the code that
    // initialises the room in the library, which in turn causes RoomListModel
    // to update the room list. So the below connection to success() will be
    // triggered after all the initialisation have happened.
    connect(job, &BaseJob::success, this, [=]
    {
        statusBar()->showMessage(tr("Joined %1 as %2")
                                 .arg(roomAlias, connection->userId()));
    });
}

void MainWindow::directChat(const QString& userId, Connection* connection) {
    if (connections.isEmpty())
    {
        QMessageBox::warning(this, tr("No connections"),
            tr("Please connect to a server before joining a room"),
            QMessageBox::Close, QMessageBox::Close);
        return;
    }

    if (!connection)
    {
        if (currentRoom && !userId.isEmpty())
        {
            connection = currentRoom->connection();
            if (connections.size() > 1)
            {
                // Double check the user intention
                QMessageBox confirmBox(QMessageBox::Question,
                    tr("Starting direct chat with %1 as %2").arg(userId, connection->userId()),
                    tr("Start direct chat with %1 under account %2?")
                        .arg(userId, connection->userId()),
                    QMessageBox::Ok|QMessageBox::Cancel, this);
                confirmBox.setButtonText(QMessageBox::Ok, tr("Start Chat"));
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

    QString userName = userId;
    while (userName.isEmpty())
    {
        QInputDialog userInput (this);
        userInput.setWindowTitle(tr("Enter user id to start direct chat."));
        userInput.setLabelText(
            tr("Enter the user id of who you would like to chat with. You will join as %1")
                .arg(connection->userId()));
        userInput.setOkButtonText(tr("Start Chat"));
        userInput.setSizeGripEnabled(true);
        // TODO: Provide a button to select the joining account
        if (userInput.exec() == QDialog::Rejected)
            return;
        // TODO: Check validity, not only non-emptyness
        if (!userInput.textValue().isEmpty())
            userName = userInput.textValue();
        else
            QMessageBox::warning(this, tr("No user id specified"),
                                 tr("Please specify non-empty user id"),
                                 QMessageBox::Close, QMessageBox::Close);
    }

    connection->requestDirectChat(userName);
    statusBar()->showMessage(tr("Starting chat with %1 as %2")
                                .arg(userName, connection->userId()), 3000);
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
    timer->connect(timer, &QTimer::timeout, this, [=] {
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
        if (error.error() == QSslError::NoSslSupport)
        {
            static bool showMsgBox = true;
            if (showMsgBox)
            {
                QMessageBox msgBox(QMessageBox::Critical, tr("No SSL support"),
                                   error.errorString(), QMessageBox::Close, this);
                msgBox.setInformativeText(
                    tr("Your SSL configuration does not allow Quaternion"
                       " to establish secure connections."));
                msgBox.exec();
                showMsgBox = false;
            }
            return;
        }
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
    Dialog authDialog(tr("Proxy needs authentication"), this,
                      Dialog::NoStatusLine, tr("Authenticate"),
                      Dialog::NoExtraButtons);
    auto layout = authDialog.addLayout<QFormLayout>();
    auto userEdit = new QLineEdit;
    layout->addRow(tr("User name"), userEdit);
    auto pwdEdit = new QLineEdit;
    pwdEdit->setEchoMode(QLineEdit::Password);
    layout->addRow(tr("Password"), pwdEdit);

    if (authDialog.exec() == QDialog::Accepted)
    {
        auth->setUser(userEdit->text());
        auth->setPassword(pwdEdit->text());
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    for (auto c: qAsConst(connections))
    {
        c->saveState();
        c->stopSync(); // Instead of deleting the connection, merely stop it
//        dropConnection(c);
    }
    for (auto c: qAsConst(logoutOnExit))
        c->logout(); // For the record, dropConnection() does it automatically
    saveSettings();
    event->accept();
}

