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

#ifdef USE_KEYCHAIN
#include <qt5keychain/keychain.h>
#endif

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
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QCompleter>
#include <QtGui/QMovie>
#include <QtGui/QPixmap>
#include <QtGui/QCloseEvent>
#include <QtGui/QDesktopServices>

using Quotient::NetworkAccessManager;
using Quotient::AccountSettings;

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
    connect( chatRoomWidget, &ChatRoomWidget::resourceRequested,
             this, &MainWindow::openResource);
    connect( chatRoomWidget, &ChatRoomWidget::joinRequested,
             this, &MainWindow::joinRoom);
    connect( roomListDock, &RoomListDock::roomSelected,
             this, &MainWindow::selectRoom);
    connect( chatRoomWidget, &ChatRoomWidget::showStatusMessage,
             statusBar(), &QStatusBar::showMessage );
    connect( userListDock, &UserListDock::userMentionRequested,
             chatRoomWidget, &ChatRoomWidget::insertMention);

    createMenu();
    systemTrayIcon = new SystemTrayIcon(this);
    systemTrayIcon->show();

    busyIndicator = new QMovie(QStringLiteral(":/busy.gif"));
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

MainWindow::~MainWindow()
{
    for (auto c: qAsConst(connections))
    {
        c->saveState();
        c->stopSync(); // Instead of deleting the connection, merely stop it
    }
    for (auto c: qAsConst(logoutOnExit))
        logout(c);
    saveSettings();
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
    using Quotient::SettingsGroup;
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
    using Quotient::Settings;

    // Connection menu
    connectionMenu = menuBar()->addMenu(tr("&Accounts"));

    connectionMenu->addAction(QIcon::fromTheme("im-user"), tr("&Login..."),
        this, [=]{ showLoginWindow(); } );

    connectionMenu->addSeparator();
    // Account submenus will be added in this place - see addConnection()
    accountListGrowthPoint = connectionMenu->addSeparator();

    // Augment poor Windows users with a handy Ctrl-Q shortcut.
    static const auto quitShortcut = QSysInfo::productType() == "windows"
            ? QKeySequence(Qt::CTRL + Qt::Key_Q) : QKeySequence::Quit;
    connectionMenu->addAction(QIcon::fromTheme("application-exit"),
        tr("&Quit"), qApp, &QApplication::quit, quitShortcut);

    // View menu
    auto viewMenu = menuBar()->addMenu(tr("&View"));

    viewMenu->addSeparator();
    auto dockPanesMenu = viewMenu->addMenu(
        QIcon::fromTheme("labplot-editvlayout"),
        tr("Dock &panels", "Panels of the dock, not 'to dock the panels'"));
    roomListDock->toggleViewAction()
            ->setStatusTip(tr("Show/hide Rooms dock panel"));
    dockPanesMenu->addAction(roomListDock->toggleViewAction());
    userListDock->toggleViewAction()
        ->setStatusTip(tr("Show/hide Users dock panel"));
    dockPanesMenu->addAction(userListDock->toggleViewAction());

    viewMenu->addSeparator();

    auto showEventsMenu = viewMenu->addMenu(tr("&Display in timeline"));
    addTimelineOptionCheckbox(
        showEventsMenu,
        tr("Normal &join/leave events"),
        tr("Show join and leave events"),
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
        tr("&No-effect activity",
           "A menu item to show/hide meaningless activity such as redacted spam"),
        tr("Show/hide meaningless activity (join-leave pairs and redacted events between)"),
        QStringLiteral("show_spammy")
    );

    viewMenu->addSeparator();

    viewMenu->addAction(tr("Edit tags order"), [this]
    {
        static const auto SettingsKey = QStringLiteral("tags_order");
        Quotient::SettingsGroup sg { QStringLiteral("UI/RoomsDock") };
        const auto savedOrder = sg.get<QStringList>(SettingsKey).join('\n');
        bool ok;
        const auto newOrder = QInputDialog::getMultiLineText(this,
                tr("Edit tags order"),
                tr("Tags can be wildcarded by * next to dot(s)\n"
                   "Clear the box to reset to defaults\n"
                   "Special tags starting with \"im.quotient.\" are: %1\n"
                   "User-defined tags should start with \"u.\"")
                .arg("invite, left, direct, none"),
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

    viewMenu->addAction(QIcon::fromTheme("format-text-blockquote"),
        tr("Edit quote style"), [this]
    {
        Quotient::SettingsGroup sg { "UI" };
        const auto type = sg.get<int>("quote_type");

        QStringList list;
        list << tr("Markdown (prepend each line with >)")
             << tr("Custom (apply regex from the config file)")
             << tr("Locale's default (%1)")
                  .arg(QLocale().quoteString(tr("Example quote")));
        bool ok;
        const auto newType = QInputDialog::getItem(this,
                tr("Edit quote style"),
                tr("Choose the default style of quotes"),
                list, type, false, &ok);

        if (ok)
            sg.setValue("quote_type", list.indexOf(newType));
    });

    // Room menu
    auto roomMenu = menuBar()->addMenu(tr("&Room"));

    createRoomAction =
        roomMenu->addAction(QIcon::fromTheme("user-group-new"),
        tr("Create &new room..."), [this]
        {
            static QPointer<CreateRoomDialog> dlg;
            summon(dlg, connections, this);
        });
    createRoomAction->setShortcut(QKeySequence::New);
    createRoomAction->setDisabled(true);
    joinAction = roomMenu->addAction(QIcon::fromTheme("list-add"),
                    tr("&Join room..."), [this] { joinRoom(); } );
    joinAction->setShortcut(Qt::CTRL + Qt::Key_J);
    joinAction->setDisabled(true);
    roomMenu->addSeparator();
    roomSettingsAction =
        roomMenu->addAction(QIcon::fromTheme("user-group-properties"),
            tr("Change room &settings..."),
            [this] {
                static QHash<QuaternionRoom*, QPointer<RoomSettingsDialog>> dlgs;
                summon(dlgs[currentRoom], currentRoom, this);
            });
    roomSettingsAction->setDisabled(true);
    roomMenu->addSeparator();
    openRoomAction = roomMenu->addAction(
        QIcon::fromTheme("document-open"), tr("Open room..."), [this] {
            openResource({}, "interactive");
        });
    openRoomAction->setStatusTip(tr("Open a room from the room list"));
    openRoomAction->setShortcut(QKeySequence::Open);
    openRoomAction->setDisabled(true);
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
        auto xchatLayout = layoutGroup->addAction("XChat");
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
        tr("Use shuttle scrollbar (requires restart)"),
        tr("Control scroll velocity instead of position with the timeline scrollbar"),
        QStringLiteral("use_shuttle_dial"), true
    );
    addTimelineOptionCheckbox(
        settingsMenu,
        tr("Load full-size images at once"),
        tr("Automatically download a full-size image instead of a thumbnail"),
        QStringLiteral("autoload_images"), true
    );
    addTimelineOptionCheckbox(
        settingsMenu,
        tr("Close to tray"),
        tr("Make close button [X] minimize to tray instead of closing main window"),
        QStringLiteral("close_to_tray"), false
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
    Quotient::SettingsGroup sg("UI/MainWindow");
    if (sg.contains("normal_geometry"))
        setGeometry(sg.value("normal_geometry").toRect());
    if (sg.value("maximized").toBool())
        showMaximized();
    if (sg.contains("window_parts_state"))
        restoreState(sg.value("window_parts_state").toByteArray());
}

void MainWindow::saveSettings() const
{
    Quotient::SettingsGroup sg("UI/MainWindow");
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
#ifdef USE_KEYCHAIN
    return loadAccessTokenFromKeyChain(account);
#else
    return loadAccessTokenFromFile(account);
#endif
}

QByteArray MainWindow::loadAccessTokenFromFile(const AccountSettings& account)
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

#ifdef USE_KEYCHAIN
QByteArray MainWindow::loadAccessTokenFromKeyChain(const AccountSettings& account)
{
    qDebug() << "Read the access token from the keychain for " << account.userId();
    QKeychain::ReadPasswordJob job(qAppName());
    job.setAutoDelete(false);
    job.setKey(account.userId());
    QEventLoop loop;
    QKeychain::ReadPasswordJob::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job.start();
    loop.exec();

    if (job.error() == QKeychain::Error::NoError)
    {
        return job.binaryData();
    }

    qWarning() << "Could not read the access token from the keychain: " << qPrintable(job.errorString());
    // no access token from the keychain, try token file
    auto accessToken = loadAccessTokenFromFile(account);
    if (job.error() == QKeychain::Error::EntryNotFound)
    {
        if(!accessToken.isEmpty())
        {
          if(QMessageBox::warning(this,
                                  tr("Access token file found"),
                                  tr("Do you want to migrate the access token for %1 "
                                     "from the file to the keychain?").arg(account.userId()),
                                  QMessageBox::Yes|QMessageBox::No) == QMessageBox::Yes)
          {
              qDebug() << "Migrating the access token from file to the keychain for " << account.userId();
              bool removed = false;
              bool saved = saveAccessTokenToKeyChain(account, accessToken, false);
              if(saved)
              {
                  QFile accountTokenFile{accessTokenFileName(account)};
                  removed = accountTokenFile.remove();
              }
              if(!(saved && removed))
              {
                  qDebug() << "Migrating the access token from the file to the keychain failed";
                  QMessageBox::warning(this,
                                       tr("Couldn't migrate access token"),
                                       tr("Quaternion couldn't migrate access token %1 "
                                          "from the file to the keychain.").arg(account.userId()),
                                       QMessageBox::Close);
              }
          }
        }
    }

  return accessToken;
}
#endif

bool MainWindow::saveAccessToken(const AccountSettings& account,
                                 const QByteArray& accessToken)
{
#ifdef USE_KEYCHAIN
    return saveAccessTokenToKeyChain(account, accessToken);
#else
    return saveAccessTokenToFile(account, accessToken);
#endif
}

bool MainWindow::saveAccessTokenToFile(const AccountSettings& account,
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

#ifdef USE_KEYCHAIN
bool MainWindow::saveAccessTokenToKeyChain(const AccountSettings& account,
                                           const QByteArray& accessToken, bool writeToFile)
{
    qDebug() << "Save the access token to the keychain for " << account.userId();
    QKeychain::WritePasswordJob job(qAppName());
    job.setAutoDelete(false);
    job.setKey(account.userId());
    job.setBinaryData(accessToken);
    QEventLoop loop;
    QKeychain::WritePasswordJob::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job.start();
    loop.exec();

    if (job.error())
    {
        qWarning() << "Could not save access token to the keychain: " << qPrintable(job.errorString());
        if (job.error() != QKeychain::Error::NoBackendAvailable &&
            job.error() != QKeychain::Error::NotImplemented &&
            job.error() != QKeychain::Error::OtherError)
        {
            if(writeToFile)
            {
                const auto button = QMessageBox::warning(this,
                                                         tr("Couldn't save access token"),
                                                         tr("Quaternion couldn't save the access token to the keychain."
                                                            " Do you want to save the access token to file %1?").arg(accessTokenFileName(account)),
                                                         QMessageBox::Yes|QMessageBox::No);
                if (button == QMessageBox::Yes) {
                  return saveAccessTokenToFile(account, accessToken);
                } else {
                  return false;
                }
            }
            return false;
        }
        else
        {
            return saveAccessTokenToFile(account, accessToken);
        }
    }

    return true;
}
#endif

void MainWindow::enableDebug()
{
    chatRoomWidget->enableDebug();
}

void MainWindow::addConnection(Connection* c, const QString& deviceName)
{
    Q_ASSERT_X(c, __FUNCTION__, "Attempt to add a null connection");

    using Room = Quotient::Room;

    c->setLazyLoading(true);
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
        [this,c] (const QString& message, const QString& details) {
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
    using namespace Quotient;
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
                tr("Request URL: %1\nResponse:\n%2")
                .arg(job->requestUrl().toDisplayString(), job->rawDataSample()));
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
    openRoomAction->setEnabled(true);
    createRoomAction->setEnabled(true);
    joinAction->setEnabled(true);

    getNewEvents(c);
}

void MainWindow::dropConnection(Connection* c)
{
    Q_ASSERT_X(c, __FUNCTION__, "Attempt to drop a null connection");

    if (currentRoom && currentRoom->connection() == c)
        selectRoom(nullptr);
    connections.removeOne(c);
    logoutOnExit.removeOne(c);
    openRoomAction->setDisabled(connections.isEmpty());
    createRoomAction->setDisabled(connections.isEmpty());
    joinAction->setDisabled(connections.isEmpty());

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
    const auto& allKnownAccounts =
        Quotient::SettingsGroup("Accounts").childGroups();
    QStringList loggedOffAccounts;
    for (const auto& a: allKnownAccounts)
    {
        AccountSettings as { a };
        // Skip accounts mentioned in active connections
        if ([&] {
                    for (auto c: connections)
                        if (as.userId() == c->userId())
                            return false;
                    return true;
                }())
            loggedOffAccounts.push_back(a);
    }

    LoginDialog dialog(this, loggedOffAccounts);
    dialog.setStatusMessage(statusMessage);
    if (dialog.exec())
        processLogin(dialog);
}

void MainWindow::showLoginWindow(const QString& statusMessage,
                                 AccountSettings& reloginAccount)
{
    LoginDialog dialog { this, reloginAccount };

    dialog.setStatusMessage(statusMessage);
    if (dialog.exec())
        processLogin(dialog);
    else
    {
        reloginAccount.clearAccessToken();
        QFile(accessTokenFileName(reloginAccount)).remove();
        // XXX: Maybe even remove the account altogether as below?
//        Quotient::SettingsGroup("Accounts").remove(reloginAccount.userId());
    }
}

void MainWindow::processLogin(LoginDialog& dialog)
{
    auto connection = dialog.releaseConnection();
    AccountSettings account(connection->userId());
    account.setKeepLoggedIn(dialog.keepLoggedIn());
    account.clearAccessToken(); // Drop the legacy - just in case
    account.setHomeserver(connection->homeserver());
    account.setDeviceId(connection->deviceId());
    account.setDeviceName(dialog.deviceName());
    if (dialog.keepLoggedIn())
    {
        if (!saveAccessToken(account, connection->accessToken()))
            qWarning() << "Couldn't save access token";
    } else
        logoutOnExit.push_back(connection);
    account.sync();

    showFirstSyncIndicator();

    auto deviceName = dialog.deviceName();
    const auto it = std::find_if(connections.cbegin(), connections.cend(),
        [connection] (Connection* c) {
            return c->userId() == connection->userId();
        });

    if (it != connections.cend())
    {
        int ret = QMessageBox::warning(this,
            tr("Logging in into a logged in account"),
            tr("You're trying to log in into an account that's "
               "already logged in. Do you want to continue?"),
            QMessageBox::Yes, QMessageBox::No);

        if (ret == QMessageBox::Yes)
            deviceName += "-" + connection->deviceId();
        else
            return;
    }
    addConnection(connection, deviceName);
}

void MainWindow::showAboutWindow()
{
    Dialog aboutDialog(tr("About Quaternion"), QDialogButtonBox::Close,
                       this, Dialog::NoStatusLine);
    auto* tabWidget = new QTabWidget();
    {
        auto *aboutPage = new QWidget();
        tabWidget->addTab(aboutPage, tr("&About"));

        auto* layout = new QVBoxLayout(aboutPage);

        auto* imageLabel = new QLabel();
        imageLabel->setPixmap(QPixmap(":/icon.png"));
        imageLabel->setAlignment(Qt::AlignHCenter);
        layout->addWidget(imageLabel);

        auto* labelString =
                new QLabel("<h1>" + QApplication::applicationDisplayName() + " v" +
                           QApplication::applicationVersion() + "</h1>");
        labelString->setAlignment(Qt::AlignHCenter);
        layout->addWidget(labelString);

        auto* linkLabel =
                new QLabel("<a href=\"https://matrix.org/docs/projects/client/quaternion.html\">"
                           + tr("Web page") + "</a>");
        linkLabel->setAlignment(Qt::AlignHCenter);
        linkLabel->setOpenExternalLinks(true);
        layout->addWidget(linkLabel);

        layout->addWidget(
                    new QLabel(tr("Copyright (C) 2019 The Quotient project.")));

#ifdef GIT_SHA1
        auto* commitLabel = new QLabel(tr("Built from Git, commit SHA:") + '\n' +
                                          QStringLiteral(GIT_SHA1));
        commitLabel->setTextInteractionFlags(Qt::TextSelectableByKeyboard|
                                             Qt::TextSelectableByMouse);
        layout->addWidget(commitLabel);
#endif

#ifdef LIB_GIT_SHA1
        auto* libCommitLabel = new QLabel(tr("Library commit SHA:") + '\n' +
                                          QStringLiteral(LIB_GIT_SHA1));
        libCommitLabel->setTextInteractionFlags(Qt::TextSelectableByKeyboard|
                                                Qt::TextSelectableByMouse);
        layout->addWidget(libCommitLabel);
#endif
    }
    {
        auto* thanksLabel = new QLabel(
            tr("Original project author: %1")
            .arg("<a href='https://github.com/Fxrh'>Felix Rohrbach</a>") + "<br/>" +
            tr("Project leader: %1")
            .arg("<a href='https://github.com/KitsuneRal'>Kitsune Ral</a>") +
            "<br/><br/>" +
            tr("Contributors:") + "<br/>" +
            "<a href='https://github.com/quotient-im/Quaternion/graphs/contributors'>" +
                tr("Quaternion contributors @ GitHub") + "</a><br/>" +
            "<a href='https://github.com/quotient-im/libQuotient/graphs/contributors'>" +
                tr("libQuotient contributors @ GitHub") + "</a><br/>" +
            "<a href='https://lokalise.co/contributors/730769035bbc328c31e863.62506391/'>" +
                tr("Quaternion translators @ Lokalise.co") + "</a><br/>" +
            tr("Special thanks to %1 for all the testing effort")
            .arg("<a href='https://matrix.to/#/@nep:pink.packageloss.eu'>nepugia</a>") +
            "<br/><br/>" +
            tr("Made with:") + "<br/>" +
            "<a href='https://www.qt.io/'>Qt 5</a><br/>"
            "<a href='https://www.qt.io/qt-features-libraries-apis-tools-and-ide/#ide'>Qt Creator</a><br/>"
            "<a href='https://www.jetbrains.com/clion/'>CLion</a><br/>"
            "<a href='https://lokalise.co'>Lokalise.co</a>"
        );
        thanksLabel->setTextInteractionFlags(Qt::TextSelectableByKeyboard|
                                             Qt::TextBrowserInteraction);
        thanksLabel->setOpenExternalLinks(true);

        tabWidget->addTab(thanksLabel, tr("&Thanks"));
    }

    aboutDialog.addWidget(tabWidget);
    aboutDialog.exec();
}

void MainWindow::invokeLogin()
{
    using namespace Quotient;
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
    AccountSettings as { c->userId() };
    c->stopSync();
    // Security over convenience: before allowing back in, remove
    // the connection from the UI
    emit c->loggedOut(); // Short circuit login error to logged-out event
    showLoginWindow(message, as);
}

void MainWindow::logout(Connection* c)
{
    Q_ASSERT_X(c, __FUNCTION__, "Logout on a null connection");

    QFile(accessTokenFileName(AccountSettings(c->userId()))).remove();

#ifdef USE_KEYCHAIN
    QKeychain::DeletePasswordJob job(qAppName());
    job.setAutoDelete(false);
    job.setKey(c->userId());
    QEventLoop loop;
    QKeychain::DeletePasswordJob::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job.start();
    loop.exec();
    if (job.error()) {
        if (job.error() == QKeychain::Error::EntryNotFound)
            qDebug() << "Access token is not in the keychain, nothing to delete";
        else {
            qWarning() << "Could not delete access token from the keychain: "
                       << qPrintable(job.errorString());
            if (job.error() != QKeychain::Error::NoBackendAvailable &&
                job.error() != QKeychain::Error::NotImplemented &&
                job.error() != QKeychain::Error::OtherError)
            {
                QMessageBox::warning(this, tr("Couldn't delete access token"),
                                     tr("Quaternion couldn't delete the access "
                                        "token from the keychain."),
                                     QMessageBox::Close);
            }
        }
    }
#endif

    c->logout();
}

QString MainWindow::resolveToId(const QString &uri) {
    auto id = uri;
    id.remove(QRegularExpression("^https://matrix.to/#/"));
    id.remove(QRegularExpression("^matrix:"));
    id.replace(QRegularExpression("^user/"), "@");
    id.replace(QRegularExpression("^roomid/"), "!");
    id.replace(QRegularExpression("^room/"), "#");
    return id;
}

Locator::ResolveResult MainWindow::openLocator(const Locator& l, const QString& action)
{
    if (!l.account)
        return Locator::NoAccount;

    auto idOrAlias = resolveToId(l.identifier);
    if (idOrAlias.startsWith('@'))
    {
        if (auto* user = l.account->user(idOrAlias))
        {
            if (action == "mention")
                chatRoomWidget->insertMention(user);
            else if (QMessageBox::question(this, tr("Open direct chat?"),
                        tr("Open direct chat with user %1?")
                        .arg(user->fullName())) == QMessageBox::Yes)
                l.account->requestDirectChat(user);
            return Locator::Success;
        }
        return Locator::MalformedId;
    }
    if (auto* room = idOrAlias.startsWith('!')
                     ? l.account->room(idOrAlias)
                     : l.account->roomByAlias(idOrAlias))
    {
        selectRoom(room);
        return Locator::Success;
    }
    return Locator::NotFound;
}

// FIXME: This should be decommissioned and inlined once we stop supporting
// legacy compilers that have BROKEN_INITIALIZER_LISTS
inline Locator makeLocator(Quotient::Connection* c, QString id)
{
#ifdef BROKEN_INITIALIZER_LISTS
    Locator l;
    l.account = c;
    l.identifier = std::move(id);
    return l;
#else
    return { c, std::move(id) };
#endif

}

MainWindow::Connection* MainWindow::getDefaultConnection() const
{
    return currentRoom ? currentRoom->connection() :
            connections.size() == 1 ? connections.front() : nullptr;
}

void MainWindow::openResource(const QString& idOrUri, const QString& action)
{
    const auto& id = resolveToId(idOrUri);
    auto l =
        action == "interactive"
        ? id.isEmpty()
          ? obtainIdentifier(getDefaultConnection(),
                             QFlag(Room | User), tr("Open room"),
                             tr("Room or user ID, room alias,\n"
                                "Matrix URI or matrix.to link"),
                             tr("Switch to room"))
          : makeLocator(nullptr, id) // Force choosing the connection
        : makeLocator(getDefaultConnection(), id);
    if (l.identifier.isEmpty())
        return;

    if (QStringLiteral("!@#$+").indexOf(l.identifier[0]) != -1)
    {
        if (action != "mention" && !l.account)
            l.account = chooseConnection(getDefaultConnection(),
                l.identifier.startsWith('@')
                ? tr("Confirm your account to open a direct chat with %1")
                  .arg(l.identifier)
                : tr("Confirm your account to open %1").arg(idOrUri));

        switch (openLocator(l, action)) {
        case Locator::MalformedId:
            break; // To the end of the function
        case Locator::NotFound:
            QMessageBox::warning(this, tr("Room not found"),
                                 tr("There's no room %1 in the room list."
                                    " Check the spelling and the account.")
                                 .arg(idOrUri));
            Q_FALLTHROUGH();
        default:
            return; // If success or no account, do nothing
        }
    }
    QMessageBox::warning(this, tr("Malformed user id"),
                         tr("%1 is not a correct Matrix identifier")
                         .arg(idOrUri),
                         QMessageBox::Close, QMessageBox::Close);
}

void MainWindow::selectRoom(Quotient::Room* r)
{
    if (r)
        qDebug() << "Opening room" << r->objectName();
    else if (currentRoom)
        qDebug() << "Closing room" << currentRoom->objectName();
    QElapsedTimer et; et.start();
    if (currentRoom)
        disconnect(currentRoom, &QuaternionRoom::displaynameChanged,
                   this, nullptr);
    currentRoom = static_cast<QuaternionRoom*>(r);
    setWindowTitle(r ? r->displayName() : QString());
    if (currentRoom)
        connect(currentRoom, &QuaternionRoom::displaynameChanged, this,
                [this] { setWindowTitle(currentRoom->displayName()); });
    chatRoomWidget->setRoom(currentRoom);
    roomListDock->setSelectedRoom(currentRoom);
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

MainWindow::Connection* MainWindow::chooseConnection(Connection* connection,
                                                     const QString& prompt)
{
    if (connections.isEmpty())
    {
        if (connection)
            return connection;

        QMessageBox::warning(this, tr("No connections"),
            tr("Please connect to a server first"),
            QMessageBox::Close, QMessageBox::Close);
        return nullptr;
    }
    if (connections.size() == 1)
        return connections.front();

    QStringList names; names.reserve(connections.size());
    int defaultIdx = -1;
    for (auto c: qAsConst(connections))
    {
        names.push_back(c->userId());
        if (c == connection)
            defaultIdx = names.size() - 1;
    }
    bool ok = false;
    const auto choice = QInputDialog::getItem(this,
            tr("Confirm account"), prompt, names, defaultIdx, false, &ok);
    if (!ok || choice.isEmpty())
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

Locator MainWindow::obtainIdentifier(Connection* initialConn,
        QFlags<CompletionType> completionType, const QString& prompt,
        const QString& label, const QString& actionName)
{
    if (connections.isEmpty())
    {
        QMessageBox::warning(this, tr("No connections"),
            tr("Please connect to a server first"),
            QMessageBox::Close, QMessageBox::Close);
        return {};
    }
    Dialog dlg(prompt, this, Dialog::NoStatusLine, actionName,
               Dialog::NoExtraButtons);
    auto* account = new QComboBox(&dlg);
    auto* identifier = new QLineEdit(&dlg);
    for (auto* c: connections)
    {
        account->addItem(c->userId(), QVariant::fromValue(c));
        if (c == initialConn)
            account->setCurrentIndex(account->count() - 1);
    }

    // Lay out controls
    auto* layout = dlg.addLayout<QFormLayout>();
    if (connections.size() > 1)
    {
        layout->addRow(tr("Account"), account);
        account->setFocus();
    } else {
        account->setCurrentIndex(0); // The only available
        account->hide(); // #523
        identifier->setFocus();
    }
    layout->addRow(label, identifier);
    setCompleter(identifier, connections[account->currentIndex()], completionType);

    auto* okButton = dlg.button(QDialogButtonBox::Ok);
    okButton->setDisabled(identifier->text().isEmpty());
    connect(account, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [&] (int index) {
            setCompleter(identifier, connections[index], completionType);
        });
    connect(identifier, &QLineEdit::textChanged, &dlg,
        [identifier,okButton] {
            okButton->setDisabled(identifier->text().isEmpty());
        });

    if (dlg.exec() == QDialog::Accepted)
    {
        return makeLocator(account->currentData().value<Connection*>(),
                           identifier->text());
    }
    return {};
}

void MainWindow::setCompleter(QLineEdit* edit, Connection* connection,
                              QFlags<CompletionType> type)
{
    QStringList list;
    if (type & Room)
    {
        for (auto* room : connection->allRooms()) {
            list << room->id();
            if (!room->canonicalAlias().isEmpty())
                list << room->canonicalAlias();
        }
    }
    if (type & User)
    {
        for (auto* user: connection->users())
            list << user->id();
    }
    list.sort();
    auto* completer = new QCompleter(list);
    completer->setFilterMode(Qt::MatchContains);
    edit->setCompleter(completer);
}

void MainWindow::joinRoom(const QString& roomIdOrAlias)
{
    auto* const defaultConnection = getDefaultConnection();
    if (defaultConnection
            && openLocator(makeLocator(defaultConnection, roomIdOrAlias))
               == Locator::Success)
        return; // Already joined room

    auto roomLocator = roomIdOrAlias.isEmpty()
            ? obtainIdentifier(defaultConnection, None,
                tr("Enter room id or alias"),
                tr("Room ID (starting with !)\nor alias (starting with #)"),
                tr("Join"))
            : makeLocator(chooseConnection(defaultConnection,
                          tr("Confirm account to join %1").arg(roomIdOrAlias))
                      , roomIdOrAlias);

    // Check whether the user cancelled room/connection dialog or no connections
    // or the room is already joined from the newly chosen account.
    if (!roomLocator.account || openLocator(roomLocator) == Locator::Success)
        return;

    using Quotient::BaseJob;
    auto* job = roomLocator.account->joinRoom(roomLocator.identifier);
    // Connection::joinRoom() already connected to success() the code that
    // initialises the room in the library, which in turn causes RoomListModel
    // to update the room list. So the below connection to success() will be
    // triggered after all the initialisation have happened.
    connect(job, &BaseJob::success, this, [this,roomLocator]
    {
        statusBar()->showMessage(
            tr("Joined %1 as %2").arg(roomLocator.identifier,
                                      roomLocator.account->userId()));
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
                      Dialog::NoStatusLine,
                      tr("Authenticate", "Authenticate with the proxy server"),
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
    if (Quotient::SettingsGroup("UI")
            .value("close_to_tray", false).toBool())
    {
        hide();
        event->ignore();
    }
    else
    {
        event->accept();
    }
}
