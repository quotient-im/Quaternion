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
#include "profiledialog.h"
#include "logindialog.h"
#include "networkconfigdialog.h"
#include "roomdialogs.h"
#include "accountselector.h"
#include "systemtrayicon.h"
#include "linuxutils.h"

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
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QCompleter>
#include <QtGui/QMovie>
#include <QtGui/QPixmap>
#include <QtGui/QCloseEvent>
#include <QtGui/QDesktopServices>

#include <set>

using Quotient::NetworkAccessManager;
using Quotient::Settings;
using Quotient::AccountSettings;
using Quotient::Uri;

MainWindow::MainWindow()
{
    Connection::setRoomType<QuaternionRoom>();

    // Bind callbacks to signals from NetworkAccessManager

    auto nam = NetworkAccessManager::instance();
    connect(nam, &QNetworkAccessManager::proxyAuthenticationRequired,
        this, &MainWindow::proxyAuthenticationRequired);
    connect(nam, &QNetworkAccessManager::sslErrors,
            this, &MainWindow::sslErrors);

    setWindowIcon(QIcon::fromTheme(appIconName(), QIcon(":/icon.png")));

    roomListDock = new RoomListDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, roomListDock);
    userListDock = new UserListDock(this);
    addDockWidget(Qt::RightDockWidgetArea, userListDock);
    chatRoomWidget = new ChatRoomWidget(this);
    setCentralWidget(chatRoomWidget);
    connect( chatRoomWidget, &ChatRoomWidget::resourceRequested,
             this, &MainWindow::openResource);
    connect(chatRoomWidget, &ChatRoomWidget::roomSettingsRequested,
            this, [this] { openRoomSettings(); });
    connect( roomListDock, &RoomListDock::roomSelected,
             this, &MainWindow::selectRoom);
    connect( chatRoomWidget, &ChatRoomWidget::showStatusMessage,
             statusBar(), &QStatusBar::showMessage );
    connect( userListDock, &UserListDock::userMentionRequested,
             chatRoomWidget, &ChatRoomWidget::insertMention);

    createMenu();
    createWinId();
    systemTrayIcon = new SystemTrayIcon(this);
    systemTrayIcon->show();

    busyIndicator = new QMovie(QStringLiteral(":/busy.gif"), {}, this);
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
    for (auto* acc: accountRegistry)
    {
        acc->saveState();
        acc->stopSync(); // Instead of deleting the connection, merely stop it
    }
    for (auto* acc: qAsConst(logoutOnExit))
        logout(acc);
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

QAction* MainWindow::addUiOptionCheckbox(QMenu* parent, const QString& text,
    const QString& statusTip, const QString& settingsKey, bool defaultValue)
{
    using Quotient::SettingsGroup;
    auto* const action =
        parent->addAction(text, this,
            [this,settingsKey] (bool checked)
            {
                SettingsGroup("UI").setValue(settingsKey, checked);
                chatRoomWidget->setRoom(nullptr);
                chatRoomWidget->setRoom(currentRoom);
            });
    action->setStatusTip(statusTip);
    action->setCheckable(true);
    action->setChecked(SettingsGroup("UI").get(settingsKey, defaultValue));
    return action;
}

static const auto ConfirmLinksSettingKey =
        QStringLiteral("/confirm_external_links");

void MainWindow::createMenu()
{
    // Connection menu
    connectionMenu = menuBar()->addMenu(tr("&Accounts"));

    connectionMenu->addAction(QIcon::fromTheme("im-user"), tr("&Login..."),
        this, [=]{ showLoginWindow(); } );

    connectionMenu->addSeparator();
    connectionMenu->addAction(
        QIcon::fromTheme("user-properties"), tr("User &profiles..."), this,
        [this, dlg = QPointer<ProfileDialog> {}]() mutable {
            summon(dlg, &accountRegistry, this);
            if (currentRoom)
                dlg->setAccount(currentRoom->connection());
        });
    connectionMenu->addSeparator();
    logoutMenu = connectionMenu->addMenu(QIcon::fromTheme("system-log-out"),
                                         tr("Log&out"));

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
    addUiOptionCheckbox(
        showEventsMenu,
        tr("Invite events"),
        tr("Show invite and withdrawn invitation events"),
        QStringLiteral("show_invite"),
        true
    );
    addUiOptionCheckbox(
        showEventsMenu,
        tr("Normal &join/leave events"),
        tr("Show join and leave events"),
        QStringLiteral("show_joinleave"),
        true
    );
    addUiOptionCheckbox(
        showEventsMenu,
        tr("Ban events"),
        tr("Show ban and unban events"),
        QStringLiteral("show_ban"),
        true
    );
    showEventsMenu->addSeparator();
    addUiOptionCheckbox(
        showEventsMenu,
        tr("&Redacted events"),
        tr("Show redacted events in the timeline as 'Redacted'"
           " instead of hiding them entirely"),
        QStringLiteral("show_redacted")
    );
    addUiOptionCheckbox(
        showEventsMenu,
        tr("Changes in display na&me"),
        tr("Show display name change"),
        QStringLiteral("show_rename"),
        true
    );
    addUiOptionCheckbox(
        showEventsMenu,
        tr("Avatar &changes"),
        tr("Show avatar update events"),
        QStringLiteral("show_avatar_update"),
        true
    );
    addUiOptionCheckbox(
        showEventsMenu,
        tr("Room alias &updates"),
        tr("Show room alias updates events"),
        QStringLiteral("show_alias_update"),
        true
    );
    addUiOptionCheckbox(
        showEventsMenu,
        //: A menu item to show/hide meaningless activity such as redacted spam
        tr("&No-effect activity"),
        tr("Show/hide meaningless activity"
           " (join-leave pairs and redacted events between)"),
        QStringLiteral("show_spammy")
    );
    addUiOptionCheckbox(
        showEventsMenu,
        tr("Un&known event types"),
        tr("Show/hide unknown event types"),
        QStringLiteral("show_unknown_events")
    );

    viewMenu->addSeparator();

    viewMenu->addAction(tr("Edit tags order"), this, [this]
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
            summon(dlg, &accountRegistry, this);
        });
    createRoomAction->setShortcut(QKeySequence::New);
    createRoomAction->setDisabled(true);
    joinAction =
        roomMenu->addAction(QIcon::fromTheme("list-add"), tr("&Join room..."),
                            [this] { openUserInput(ForJoining); });
    joinAction->setShortcut(Qt::CTRL + Qt::Key_J);
    joinAction->setDisabled(true);
    roomMenu->addSeparator();
    roomSettingsAction =
        roomMenu->addAction(QIcon::fromTheme("user-group-properties"),
                            tr("Change room &settings..."), this,
                            [this] { openRoomSettings(); });
    roomSettingsAction->setDisabled(true);
    roomMenu->addSeparator();
    openRoomAction = roomMenu->addAction(QIcon::fromTheme("document-open"),
                                         tr("Open room..."),
                                         [this] { openUserInput(); });
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
    helpMenu->addAction(QIcon::fromTheme("help-about"), tr("&About Quaternion"),
                        [this] { showAboutWindow(); });
    helpMenu->addAction(QIcon::fromTheme("help-about-qt"), tr("About &Qt"),
                        [this] { QMessageBox::aboutQt(this); });

    {
        auto notifGroup = new QActionGroup(this);
        connect(notifGroup, &QActionGroup::triggered, this,
            [] (QAction* notifAction)
            {
                notifAction->setChecked(true);
                Settings().setValue("UI/notifications",
                                    notifAction->data().toString());
            });

        static const auto MinSetting = QStringLiteral("none");
        static const auto GentleSetting = QStringLiteral("non-intrusive");
        static const auto LoudSetting = QStringLiteral("intrusive");
        auto noNotif = notifGroup->addAction(tr("&Highlight only"));
        noNotif->setData(MinSetting);
        noNotif->setStatusTip(tr("Notifications are entirely suppressed"));
        auto gentleNotif = notifGroup->addAction(tr("&Non-intrusive"));
        gentleNotif->setData(GentleSetting);
        gentleNotif->setStatusTip(
            tr("Show notifications but do not activate the window"));
        auto fullNotif = notifGroup->addAction(tr("&Full"));
        fullNotif->setData(LoudSetting);
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

        const auto curSetting =
                Settings().get("UI/notifications", LoudSetting);
        if (curSetting == MinSetting)
            noNotif->setChecked(true);
        else if (curSetting == GentleSetting)
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
#if defined Q_OS_UNIX && !defined Q_OS_MAC
    addUiOptionCheckbox(
        settingsMenu,
        tr("Use Breeze style (requires restart)"),
        tr("Force use Breeze style and icon theme"),
        QStringLiteral("use_breeze_style"), inFlatpak()
    );
#endif
    addUiOptionCheckbox(
        settingsMenu,
        tr("Use shuttle scrollbar (requires restart)"),
        tr("Control scroll velocity instead of position"
           " with the timeline scrollbar"),
        QStringLiteral("use_shuttle_dial"), true
    );
    addUiOptionCheckbox(
        settingsMenu,
        tr("Load full-size images at once"),
        tr("Automatically download a full-size image instead of a thumbnail"),
        QStringLiteral("autoload_images"), true
    );
    addUiOptionCheckbox(
        settingsMenu,
        tr("Close to tray"),
        tr("Make close button [X] minimize to tray instead of closing main window"),
        QStringLiteral("close_to_tray"), false
    );

    confirmLinksAction =
        addUiOptionCheckbox(
            settingsMenu,
            tr("Confirm opening external links"),
            tr("Show a confirmation box before opening non-Matrix links"
               " in an external application"),
            ConfirmLinksSettingKey, true);

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

template <class KeySourceT>
inline QString accessTokenKey(const KeySourceT& source, bool legacyLocation)
{
    auto k = source.userId();
    if (!legacyLocation) {
        if (source.deviceId().isEmpty())
            qWarning() << "Device id on the account is not set";
        else
            k += '-' % source.deviceId();
    }
    return k;
}

template <class JobT>
inline std::unique_ptr<JobT> makeKeychainJob(const QString& appName,
                                             const QString& key,
                                             bool legacyLocation = false)
{
    auto slotName = appName;
    if (!legacyLocation)
        slotName += " access token for " % key;
    auto j = std::make_unique<JobT>(slotName);
    j->setAutoDelete(false);
    j->setKey(key);
    return j;
}

template <class KeySourceT>
inline QString accessTokenFileName(const KeySourceT& account,
                                   bool legacyLocation = false)
{
    auto fileName = accessTokenKey(account, legacyLocation);
    fileName.replace(':', '_');
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
           % '/' % fileName;
}

class AccessTokenFile : public QFile { // clazy:exclude=missing-qobject-macro
    bool legacyLocation = false;

public:
    template <class KeySourceT>
    explicit AccessTokenFile(const KeySourceT& source, OpenMode mode = ReadOnly)
    {
        Q_ASSERT(mode == ReadOnly || mode == WriteOnly);
        if (mode == WriteOnly) {
            remove(accessTokenFileName(source, true));
            setFileName(accessTokenFileName(source, false));
            remove();
            const auto fileDir = QFileInfo(*this).dir();
            if (fileDir.exists() || fileDir.mkpath("."))
                open(QFile::WriteOnly);
            return;
        }
        for (bool getLegacyLocation: { false, true }) {
            setFileName(accessTokenFileName(source, getLegacyLocation));
            if (open(QFile::ReadOnly)) {
                if (size() < 1024) {
                    qDebug() << "Found access token file at" << fileName();
                    legacyLocation = getLegacyLocation;
                    return;
                }
                qWarning() << "File" << fileName() << "is" << size()
                           << "bytes long - too long for a token, ignoring it.";
            } else
                qWarning() << "Could not open access token file" << fileName();
            close();
        }
    }
    [[nodiscard]] bool isAtLegacyLocation() const { return legacyLocation; }
};

QByteArray MainWindow::loadAccessToken(const AccountSettings& account)
{
#ifdef USE_KEYCHAIN
    if (Settings().get("UI/use_keychain", true))
        return loadAccessTokenFromKeyChain(account);

    qDebug() << "Explicit opt-out from keychain by user setting";
#endif
    return AccessTokenFile(account).readAll();
}

#ifdef USE_KEYCHAIN
QByteArray MainWindow::loadAccessTokenFromKeyChain(const AccountSettings& account)
{
    using namespace QKeychain;
    auto lastError = Error::OtherError;
    bool legacyLocation = true;
    do {
        legacyLocation = !legacyLocation; // Start with non-legacy

        const auto& key = accessTokenKey(account, legacyLocation);
        qDebug().noquote() << "Reading the access token from the keychain for"
                           << key;
        auto job =
            makeKeychainJob<ReadPasswordJob>(qAppName(), key, legacyLocation);
        QEventLoop loop;
        connect(job.get(), &Job::finished, &loop, &QEventLoop::quit);
        job->start();
        loop.exec();

        if (job->error() == Error::NoError) {
            auto token = job->binaryData();
            if (legacyLocation) {
                qDebug() << "Migrating the token to the new keychain slot";
                if (saveAccessTokenToKeyChain(account, token, false)) {
                    auto* delJob = new DeletePasswordJob(qAppName());
                    delJob->setAutoDelete(true);
                    delJob->setKey(accessTokenKey(account, true));
                    connect(delJob, &Job::finished, this, [delJob] {
                        if (delJob->error() != Error::NoError)
                            qWarning().noquote()
                                << "Cleanup of the old keychain slot failed:"
                                << delJob->errorString();
                    });
                    delJob->start(); // Run async and move on
                }
            }
            return token;
        }

        qWarning().noquote() << "Could not read" << job->service()
                             << "from the keychain:" << job->errorString();
        lastError = job->error();
    } while (!legacyLocation); // Exit once the legacy round is through

    // Try token file
    AccessTokenFile atf(account);
    const auto& accessToken = atf.readAll();
    // Only offer migration if QtKeychain is usable but doesn't have the entry
    if (lastError == Error::EntryNotFound && !accessToken.isEmpty()
        && QMessageBox::warning(
               this, tr("Access token file found"),
               tr("Do you want to migrate the access token for %1 "
                  "from the file to the keychain?")
                   .arg(account.userId()),
               QMessageBox::Yes | QMessageBox::No)
               == QMessageBox::Yes) {
        qInfo() << "Migrating the access token for" << account.userId()
                << "from the file to the keychain";
        if (saveAccessTokenToKeyChain(account, accessToken, false)) {
            if (!atf.remove())
                qWarning()
                    << "Could not remove the access token after migration";
        } else {
            qWarning() << "Migration of the access token failed";
            QMessageBox::warning(this, tr("Couldn't migrate access token"),
                tr("Quaternion couldn't migrate access token for %1 "
                   "from the file to the keychain.")
                    .arg(account.userId()),
                QMessageBox::Close);
        }
    }
    return accessToken;
}
#endif

bool MainWindow::saveAccessToken(const AccountSettings& account,
                                 const QByteArray& accessToken)
{
#ifdef USE_KEYCHAIN
    if (Settings().get("UI/use_keychain", true))
        return saveAccessTokenToKeyChain(account, accessToken);

    qDebug() << "Explicit opt-out from keychain by user setting";
#endif // fall through to non QtKeychain-specific code
    return saveAccessTokenToFile(account, accessToken);
}

bool MainWindow::saveAccessTokenToFile(const AccountSettings& account,
                                       const QByteArray& accessToken)
{
    // (Re-)Make a dedicated file for access_token.
    AccessTokenFile accountTokenFile(account, QFile::WriteOnly);
    if (!accountTokenFile.isOpen()) {
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
                                           const QByteArray& accessToken,
                                           bool writeToFile)
{
    using namespace QKeychain;
    const auto key = accessTokenKey(account, false);
    qDebug().noquote() << "Save the access token to the keychain for" << key;
    auto job = makeKeychainJob<WritePasswordJob>(qAppName(), key);
    job->setBinaryData(accessToken);
    QEventLoop loop;
    connect(job.get(), &Job::finished, &loop, &QEventLoop::quit);
    job->start();
    loop.exec();

    if (!job->error())
        return true;

    qWarning().noquote() << "Could not save access token to the keychain:"
                         << job->errorString();
    if (job->error() != Error::NoBackendAvailable
        && job->error() != Error::NotImplemented
        && job->error() != Error::OtherError) {
        if (writeToFile) {
            const auto button = QMessageBox::warning(
                this, tr("Couldn't save access token"),
                tr("Quaternion couldn't save the access token to the keychain."
                   " Do you want to save the access token to file %1?")
                    .arg(accessTokenFileName(account)),
                QMessageBox::Yes | QMessageBox::No);
            if (button == QMessageBox::Yes)
                return saveAccessTokenToFile(account, accessToken);
        }
        return false;
    }
    return saveAccessTokenToFile(account, accessToken);
}
#endif

void MainWindow::addConnection(Connection* c, const QString& deviceName)
{
    Q_ASSERT_X(c, __FUNCTION__, "Attempt to add a null connection");

    using Room = Quotient::Room;

    c->setLazyLoading(true);
    accountRegistry.add(c);

    roomListDock->addConnection(c);

    c->syncLoop();
    connect(c, &Connection::syncDone, this, [this, c, counter = 0]() mutable {
        if (counter == 0) {
            firstSyncOver(c);
            statusBar()->showMessage(
                tr("First sync completed for %1", "%1 is user id")
                    .arg(c->userId()),
                3000);
        }

        // Borrowed the logic from Quiark's code in Tensor to cache not too
        // aggressively and not on the first sync.
        if (++counter % 17 == 2)
            c->saveState();
    } );
    connect( c, &Connection::loggedOut, this, [=]
    {
        statusBar()->showMessage(tr("Logged out as %1").arg(c->userId()), 3000);
        accountRegistry.drop(c);
        dropConnection(c);
    });
    connect( c, &Connection::networkError, this, [=]{ networkError(c); } );
    connect( c, &Connection::syncError, this,
        [this,c] (const QString& message, const QString& details) {
            QMessageBox msgBox(QMessageBox::Warning, tr("Sync failed"),
                accountRegistry.size() > 1
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
                c->syncLoop();
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

    // Update the menu

    QString accountCaption = c->userId();
    if (!deviceName.isEmpty())
        accountCaption += '/' % deviceName;
    QString menuCaption = accountCaption;
    if (accountRegistry.size() < 10)
        menuCaption.prepend('&' % QString::number(accountRegistry.size()) % ' ');
    auto logoutAction = logoutMenu->addAction(menuCaption, [=] { logout(c); });
    connect(c, &Connection::destroyed, logoutMenu,
            std::bind(&QMenu::removeAction, logoutMenu, logoutAction));
    openRoomAction->setEnabled(true);
    createRoomAction->setEnabled(true);
    joinAction->setEnabled(true);
}

void MainWindow::dropConnection(Connection* c)
{
    Q_ASSERT_X(c, __FUNCTION__, "Attempt to drop a null connection");

    if (currentRoom && currentRoom->connection() == c)
        selectRoom(nullptr);
    accountRegistry.drop(c);

    logoutOnExit.removeOne(c);
    const auto noMoreAccounts = accountRegistry.isEmpty();
    openRoomAction->setDisabled(noMoreAccounts);
    createRoomAction->setDisabled(noMoreAccounts);
    joinAction->setDisabled(noMoreAccounts);

    Q_ASSERT(!logoutOnExit.contains(c) && !c->syncJob());
    c->deleteLater();
}

void MainWindow::showFirstSyncIndicator()
{
    busyLabel->show();
    busyIndicator->start();
    statusBar()->showMessage("Syncing, please wait");
}

void MainWindow::firstSyncOver(Connection *c)
{
    Q_ASSERT(c != nullptr);
    firstSyncing.removeOne(c);
    if (firstSyncing.empty()) {
        busyLabel->hide();
        busyIndicator->stop();
    }
    qDebug() << "Connections still in first sync: " << firstSyncing.size();
}

void MainWindow::showLoginWindow(const QString& statusMessage)
{
    const auto& allKnownAccounts =
        Quotient::SettingsGroup("Accounts").childGroups();
    QStringList loggedOffAccounts;
    for (const auto& a: allKnownAccounts)
        // Skip already logged in accounts
        if (!accountRegistry.isLoggedIn(AccountSettings(a).userId()))
            loggedOffAccounts.push_back(a);

    doOpenLoginDialog(new LoginDialog(statusMessage, this, loggedOffAccounts));
}

void MainWindow::showLoginWindow(const QString& statusMessage,
                                 const QString& userId)
{
    auto* reloginAccount = new AccountSettings(userId);
    auto* dialog = new LoginDialog(statusMessage, this, *reloginAccount);
    reloginAccount->setParent(dialog); // => Delete with the dialog box
    doOpenLoginDialog(dialog);
    connect(dialog, &QDialog::rejected, this, [reloginAccount] {
        reloginAccount->clearAccessToken();
        AccessTokenFile(*reloginAccount).remove();
        // XXX: Maybe even remove the account altogether as below?
        // Quotient::SettingsGroup("Accounts").remove(reloginAccount->userId());
    });
}

void MainWindow::doOpenLoginDialog(LoginDialog* dialog)
{
    dialog->open();
    // See #666: WA_DeleteOnClose kills the dialog object too soon,
    // invalidating the connection object before it's released to the local
    // variable below; so the dialog object is explicitly deleted instead of
    // using WA_DeleteOnClose automagic.
    connect(dialog, &QDialog::accepted, this, [this, dialog] {
        auto connection = dialog->releaseConnection();
        AccountSettings account(connection->userId());
        account.setKeepLoggedIn(dialog->keepLoggedIn());
        account.clearAccessToken(); // Drop the legacy - just in case
        account.setHomeserver(connection->homeserver());
        account.setDeviceId(connection->deviceId());
        account.setDeviceName(dialog->deviceName());
        if (dialog->keepLoggedIn()) {
            if (!saveAccessToken(account, connection->accessToken()))
                qWarning() << "Couldn't save access token";
        } else
            logoutOnExit.push_back(connection);
        account.sync();

        auto deviceName = dialog->deviceName();
        dialog->deleteLater();

        firstSyncing.push_back(connection);
        showFirstSyncIndicator();

        if (accountRegistry.isLoggedIn(connection->userId())) {
            if (QMessageBox::warning(
                    this, tr("Logging in into a logged in account"),
                    tr("You're trying to log in into an account that's "
                       "already logged in. Do you want to continue?"),
                    QMessageBox::Yes, QMessageBox::No)
                != QMessageBox::Yes)
                return;

            deviceName += "-" + connection->deviceId();
        }
        addConnection(connection, deviceName);
    });
    connect(dialog, &QDialog::rejected, dialog, &QObject::deleteLater);
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
                           % tr("Web page") % "</a>");
        linkLabel->setAlignment(Qt::AlignHCenter);
        linkLabel->setOpenExternalLinks(true);
        layout->addWidget(linkLabel);

        layout->addWidget(
                    new QLabel("Copyright (C) 2016-2021 "
                               % tr("Quaternion project contributors")));

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
            .arg("<a href='https://github.com/Fxrh'>"
                 % tr("Felix Rohrbach") % "</a>") % "<br/>" %
            tr("Project leader: %1")
            .arg("<a href='https://github.com/KitsuneRal'>"
                 % tr("Alexey \"Kitsune\" Rusakov") % "</a>") %
            "<br/><br/>" %
            tr("Contributors:") % "<br/>" %
            "<a href='https://github.com/quotient-im/Quaternion/graphs/contributors'>" %
                tr("Quaternion contributors @ GitHub") % "</a><br/>" +
            "<a href='https://github.com/quotient-im/libQuotient/graphs/contributors'>" %
                tr("libQuotient contributors @ GitHub") % "</a><br/>" %
            "<a href='https://lokalise.com/contributors/730769035bbc328c31e863.62506391/'>" %
                tr("Quaternion translators @ Lokalise.co") % "</a><br/>" %
            tr("Special thanks to %1 for all the testing effort")
            .arg("<a href='mailto:nep-quaternion@packageloss.eu'>nephele</a>") %
            "<br/><br/>" %
            tr("Made with:") % "<br/>" %
            "<a href='https://www.qt.io/'>Qt 5</a><br/>"
            "<a href='https://www.qt.io/product/development-tools'>Qt Creator</a><br/>"
            "<a href='https://www.jetbrains.com/clion/'>CLion</a><br/>"
            "<a href='https://lokalise.com'>Lokalise</a>"
            "<a href='https://cloudsmith.io'>Cloudsmith<a>"
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
            firstSyncing.push_back(c);
            auto deviceName = account.deviceName();
            connect(c, &Connection::connected, this,
                [=] {
                    c->loadState();
                    addConnection(c, deviceName);
                });
            connect(c, &Connection::resolveError, this, [this, c] {
                firstSyncOver(c);
                statusBar()->showMessage(
                    tr("Failed to resolve server %1").arg(c->domain()), 4000);
            });
            c->assumeIdentity(account.userId(), accessToken, account.deviceId());
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
    c->stopSync();
    // Security over convenience: before allowing back in, remove
    // the connection from the UI
    emit c->loggedOut(); // Short circuit login error to logged-out event
    showLoginWindow(message, c->userId());
}

void MainWindow::logout(Connection* c)
{
    Q_ASSERT_X(c, __FUNCTION__, "Logout on a null connection");

    AccessTokenFile(*c).remove();

#ifdef USE_KEYCHAIN
    if (Settings().get("UI/use_keychain", true))
        for (bool legacyLocation: { false, true }) {
            using namespace QKeychain;
            auto* job = new DeletePasswordJob(qAppName());
            job->setAutoDelete(true);
            job->setKey(accessTokenKey(*c, legacyLocation));
            connect(job, &Job::finished, this, [this, job] {
                switch (job->error()) {
                case Error::EntryNotFound:
                    qDebug() << "Access token is not in the keychain, nothing "
                                "to delete";
                    [[fallthrough]];
                case Error::NoError:
                    return;
                // Actual errors follow
                case Error::NoBackendAvailable:
                case Error::NotImplemented:
                case Error::OtherError:
                    break;
                default:
                    QMessageBox::warning(
                        this, tr("Couldn't delete access token"),
                        tr("Quaternion couldn't delete the access "
                           "token from the keychain."),
                        QMessageBox::Close);
                }
                qWarning()
                    << "Could not delete access token from the keychain: "
                    << qUtf8Printable(job->errorString());
            });
            job->start();
        }
#endif

    c->logout();
}

Quotient::UriResolveResult MainWindow::visitUser(Quotient::User* user,
                                                 const QString& action)
{
    if (action == "mention" || action.isEmpty())
        chatRoomWidget->insertMention(user);
    // action=_interactive is checked in openResource() and
    // converted to "chat" in openUserInput()
    else if (action == "_interactive"
             || (action == "chat"
                 && QMessageBox::question(this, tr("Open direct chat?"),
                                          tr("Open direct chat with user %1?")
                                              .arg(user->fullName()))
                        == QMessageBox::Yes))
        user->requestDirectChat();
    else
        return Quotient::IncorrectAction;

    return Quotient::UriResolved;
}

void MainWindow::visitRoom(Quotient::Room* room, const QString& eventId)
{
    selectRoom(room);
    if (!eventId.isEmpty())
        chatRoomWidget->spotlightEvent(eventId);
}

void MainWindow::joinRoom(Quotient::Connection* account,
                          const QString& roomAliasOrId,
                          const QStringList& viaServers)
{
    auto* job =
        account->joinRoom(QUrl::toPercentEncoding(roomAliasOrId), viaServers);
    // Connection::joinRoom() already connected to success() the code that
    // initialises the room in the library, which in turn causes RoomListModel
    // to update the room list. So the below connection to success() will be
    // triggered after all the initialisation have happened.
    connect(job, &Quotient::BaseJob::success, this,
            [this, account, roomAliasOrId] {
                statusBar()->showMessage(
                    tr("Joined %1 as %2").arg(roomAliasOrId, account->userId()));
            });
}

bool MainWindow::visitNonMatrix(const QUrl& url)
{
    // Return true if the user cancels, treating it as an alternative normal
    // flow (rather than an abnormal flow when the navigation itself fails).
    auto doVisit = [this, url] {
        if (!QDesktopServices::openUrl(url))
            QMessageBox::warning(this, tr("No application for the link"),
                                 tr("Your operating system could not find an "
                                    "application for the link."));
    };
    using Quotient::SettingsGroup;
    if (SettingsGroup("UI").get(ConfirmLinksSettingKey, true)) {
        auto* confirmation = new QMessageBox(
            QMessageBox::Warning, tr("External link confirmation"),
            tr("An external application will be opened to visit a "
               "non-Matrix link:\n\n%1\n\nIs that right?")
                .arg(url.toDisplayString()),
            QMessageBox::Ok | QMessageBox::Cancel, this, Qt::Dialog);
        confirmation->setDefaultButton(nullptr);
        confirmation->setCheckBox(new QCheckBox(tr("Do not ask again")));
        confirmation->setWindowModality(Qt::WindowModal);
        confirmation->show();
        connect(confirmation, &QDialog::finished,
                this, [this,doVisit,confirmation](int result) {
                    const bool doNotAsk =
                        confirmation->checkBox()->checkState() == Qt::Checked;
                    if (doNotAsk)
                        confirmLinksAction->setDisabled(true);

                    SettingsGroup("UI").setValue(ConfirmLinksSettingKey,
                                                 !doNotAsk);
                    if (result == QMessageBox::Ok)
                        doVisit();
                });
    } else
        doVisit();
    return true;
}

MainWindow::Connection* MainWindow::getDefaultConnection() const
{
    return currentRoom ? currentRoom->connection() :
            accountRegistry.size() == 1 ? accountRegistry.front() : nullptr;
}

void MainWindow::openResource(const QString& idOrUri, const QString& action)
{
    Uri uri { idOrUri };
    if (!uri.isValid()) {
        QMessageBox::warning(
            this, tr("Malformed or empty Matrix id"),
            tr("%1 is not a correct Matrix identifier").arg(idOrUri),
            QMessageBox::Close, QMessageBox::Close);
        return;
    }
    auto* account = getDefaultConnection();
    if (uri.type() != Uri::NonMatrix) {
        if (!account) {
            showLoginWindow(tr("Please connect to a server"));
            return;
        }
        if (!action.isEmpty())
            uri.setAction(action);

        if (uri.action() == "join"
            && currentRoom
            // NB: We can't reliably check aliases for being in the upgrade chain
            && currentRoom->successorId() != uri.primaryId()
            && currentRoom->predecessorId() != uri.primaryId())
            account = chooseConnection(
                account, tr("Confirm account to join %1").arg(uri.primaryId()));
        else if (uri.action() == "_interactive")
            account = chooseConnection(
                account,
                uri.type() == Uri::UserId
                    ? tr("Confirm your account to open a direct chat with %1")
                          .arg(uri.primaryId())
                    : tr("Confirm your account to open %1").arg(idOrUri));
        if (!account)
            return; // The user cancelled the confirmation dialog
    }
    const auto result = visitResource(account, uri);
    if (result == Quotient::CouldNotResolve)
        QMessageBox::warning(this, tr("Room not found"),
                             tr("There's no room %1 in the room list."
                                " Check the spelling and the account.")
                                 .arg(idOrUri));
    else // Invalid cases should have been eliminated earlier
        Q_ASSERT(result == Quotient::UriResolved);
}

void MainWindow::openRoomSettings(QuaternionRoom* r)
{
    if (!r)
        r = currentRoom;
    static std::unordered_map<QuaternionRoom*, QPointer<RoomSettingsDialog>> dlgs;
    const auto [it, inserted] = dlgs.try_emplace(r);
    summon(it->second, r, this);
    if (inserted)
        connect(it->second, &QObject::destroyed, [r] { dlgs.erase(r); });
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
    Q_ASSERT(!accountRegistry.isEmpty());
    if (accountRegistry.size() == 1)
        return accountRegistry.front();

    QStringList names; names.reserve(accountRegistry.size());
    int defaultIdx = -1;
    for (auto c: accountRegistry)
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

    for (auto c: accountRegistry)
        if (c->userId() == choice)
        {
            connection = c;
            break;
        }
    Q_ASSERT(connection);
    return connection;
}

void MainWindow::openUserInput(bool forJoining)
{
    if (accountRegistry.isEmpty()) {
        showLoginWindow(tr("Please connect to a server"));
        return;
    }

    struct D {
        QString dlgTitle;
        QString dlgText;
        QString actionText;
    };
    static const D map[] = {
        { tr("Open room"),
          tr("Room or user ID, room alias,\nMatrix URI or matrix.to link"),
          tr("Go to room") },
        { tr("Join room"),
          tr("Room ID (starting with !)\nor alias (starting with #)"),
          tr("Join room") }
    };
    const auto& entry = map[forJoining];

    Dialog dlg(entry.dlgTitle, this, Dialog::NoStatusLine, entry.actionText,
               Dialog::NoExtraButtons);
    auto* accountChooser = new AccountSelector(&accountRegistry);
    auto* identifier = new QLineEdit(&dlg);
    auto* defaultConn = getDefaultConnection();
    accountChooser->setAccount(defaultConn);

    // Lay out controls
    auto* layout = dlg.addLayout<QFormLayout>();
    if (accountRegistry.size() > 1)
    {
        layout->addRow(tr("Account"), accountChooser);
        accountChooser->setFocus();
    } else {
        accountChooser->setCurrentIndex(0); // The only available
        accountChooser->hide(); // #523
        identifier->setFocus();
    }
    layout->addRow(entry.dlgText, identifier);

    if (!forJoining) {
        const auto setCompleter = [identifier](Connection* connection) {
            if (!connection) {
                identifier->setCompleter(nullptr);
                return;
            }
            QStringList completions;
            const auto& allRooms = connection->allRooms();
            const auto& users = connection->users();
            // Assuming that roughly half of rooms in the room list have
            // a canonical alias; this may be quite a bit off but is better
            // than not reserving at all
            completions.reserve(allRooms.size() * 3 / 2 + users.size());
            for (auto* room: allRooms) {
                completions << room->id();
                if (!room->canonicalAlias().isEmpty())
                    completions << room->canonicalAlias();
            }

            for (auto* user: users)
                completions << user->id();

            completions.sort();
            completions.erase(std::unique(completions.begin(), completions.end()),
                              completions.end());

            auto* completer = new QCompleter(completions);
            completer->setFilterMode(Qt::MatchContains);
            identifier->setCompleter(completer);
        };

        setCompleter(accountChooser->currentAccount());
        connect(accountChooser, &AccountSelector::currentAccountChanged,
                identifier, setCompleter);
    }

    const auto getUri = [identifier]() -> Uri {
        return identifier->text().trimmed();
    };
    auto* okButton = dlg.button(QDialogButtonBox::Ok);
    okButton->setDisabled(true);
    connect(identifier, &QLineEdit::textChanged, &dlg,
            [getUri, okButton, buttonText = entry.actionText] {
                switch (getUri().type()) {
                case Uri::RoomId:
                case Uri::RoomAlias:
                    okButton->setEnabled(true);
                    okButton->setText(buttonText);
                    break;
                case Uri::UserId:
                    okButton->setEnabled(true);
                    okButton->setText(tr("Chat with user",
                                         "On a button in 'Open room' dialog"
                                         " when a user identifier is entered"));
                    break;
                default:
                    okButton->setDisabled(true);
                    okButton->setText(
                        tr("Can't open",
                           "On a disabled button in 'Open room' dialog when"
                           " an invalid/unsupported URI is entered"));
                }
            });

    if (dlg.exec() != QDialog::Accepted)
        return;

    auto uri = getUri();
    if (forJoining)
        uri.setAction("join");
    else if (uri.type() == Uri::UserId
             && (uri.action().isEmpty() || uri.action() == "_interactive"))
        uri.setAction("chat"); // The default action for users is "mention"

    switch (visitResource(accountChooser->currentAccount(), uri))
    {
    case Quotient::UriResolved:
        break;
    case Quotient::CouldNotResolve:
        QMessageBox::warning(
            this, tr("Could not resolve id"),
            (uri.type() == Uri::NonMatrix
                 ? tr("Could not find an external application to open the URI:")
                 : tr("Could not resolve Matrix identifier"))
                + "\n\n" + uri.toDisplayString());
        break;
    case Quotient::IncorrectAction:
        QMessageBox::warning(
            this, tr("Incorrect action on a Matrix resource"),
            tr("The URI contains an action '%1' that cannot be applied"
               " to Matrix resource %2")
                .arg(uri.action(), uri.toDisplayString(QUrl::RemoveQuery)));
        break;
    default:
        Q_ASSERT(false); // No other values should occur
    }
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
    connect(timer, &QTimer::timeout, this, [this,c,timer] {
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
