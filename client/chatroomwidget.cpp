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

#include "chatroomwidget.h"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QLabel>

#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickView>
#include <QtQuick/QQuickItem>
#include <QtCore/QRegularExpression>
#include <QtCore/QStringBuilder>

#include "lib/events/roommessageevent.h"
#include "lib/user.h"
#include "lib/connection.h"
#include "lib/settings.h"
#include "models/messageeventmodel.h"
#include "imageprovider.h"
#include "chatedit.h"

ChatRoomWidget::ChatRoomWidget(QWidget* parent)
    : QWidget(parent)
    , m_currentRoom(nullptr)
    , readMarkerOnScreen(false)
{
    qmlRegisterType<QuaternionRoom>();
    qmlRegisterType<QMatrixClient::Settings>("QMatrixClient", 1, 0, "Settings");
    m_messageModel = new MessageEventModel(this);

    m_roomAvatar = new QLabel();
    m_roomAvatar->setPixmap({});
    m_roomAvatar->setFrameStyle(QFrame::Sunken);

    m_topicLabel = new QLabel();
    m_topicLabel->setTextFormat(Qt::RichText);
    m_topicLabel->setWordWrap(true);
    m_topicLabel->setTextInteractionFlags(Qt::TextBrowserInteraction
                                          |Qt::TextSelectableByKeyboard);
    m_topicLabel->setOpenExternalLinks(true);

    auto topicSeparator = new QFrame();
    topicSeparator->setFrameShape(QFrame::HLine);

    m_quickView = new QQuickView();

    m_imageProvider = new ImageProvider(nullptr); // No connection yet
    m_quickView->engine()->addImageProvider("mtx", m_imageProvider);

    QWidget* container = QWidget::createWindowContainer(m_quickView, this);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QQmlContext* ctxt = m_quickView->rootContext();
    ctxt->setContextProperty("messageModel", m_messageModel);
    ctxt->setContextProperty("controller", this);
    ctxt->setContextProperty("debug", QVariant(false));
    m_quickView->setSource(QUrl("qrc:///qml/chat.qml"));
    m_quickView->setResizeMode(QQuickView::SizeRootObjectToView);

    m_currentlyTyping = new QLabel();
    m_currentlyTyping->setWordWrap(true);

    m_chatEdit = new ChatEdit(this);
    m_chatEdit->setPlaceholderText(tr("Send a message (unencrypted)..."));
    m_chatEdit->setAcceptRichText(false);
    connect( m_chatEdit, &KChatEdit::returnPressed, this, &ChatRoomWidget::sendInput );
    connect(m_chatEdit, &ChatEdit::proposedCompletion, this,
            [=](const QStringList& matches, int pos)
            {
                m_currentlyTyping->setText(
                    tr("<i>Tab Completion (next: %1)</i>")
                    .arg( QStringList(matches.mid(pos, 5)).join(", ") ) );
            });
    connect(m_chatEdit, &ChatEdit::cancelledCompletion,
            this, &ChatRoomWidget::typingChanged);

    auto layout = new QVBoxLayout();

    auto headerLayout = new QHBoxLayout;
    headerLayout->addWidget(m_roomAvatar);
    headerLayout->addWidget(m_topicLabel, 1);
    layout->addLayout(headerLayout);

    layout->addWidget(topicSeparator);
    layout->addWidget(container);
    layout->addWidget(m_currentlyTyping);
    layout->addWidget(m_chatEdit);
    setLayout(layout);
}

void ChatRoomWidget::enableDebug()
{
    QQmlContext* ctxt = m_quickView->rootContext();
    ctxt->setContextProperty("debug", true);
}

void ChatRoomWidget::setRoom(QuaternionRoom* room)
{
    if (m_currentRoom == room) {
        m_chatEdit->setFocus();
        return;
    }

    if( m_currentRoom )
    {
        m_currentRoom->setCachedInput( m_chatEdit->toPlainText() );
        m_currentRoom->setShown(false);
        roomHistories.insert(m_currentRoom, m_chatEdit->history());
        m_currentRoom->connection()->disconnect(this);
        m_currentRoom->disconnect( this );
    }
    readMarkerOnScreen = false;
    maybeReadTimer.stop();
    indicesOnScreen.clear();
    m_chatEdit->cancelCompletion();

    m_currentRoom = room;
    if( m_currentRoom )
    {
        using namespace QMatrixClient;
        m_imageProvider->setConnection(room->connection());
        m_chatEdit->setText( m_currentRoom->cachedInput() );
        m_chatEdit->setHistory(roomHistories.value(m_currentRoom));
        m_chatEdit->setFocus();
        m_chatEdit->moveCursor(QTextCursor::End);
        connect( m_currentRoom, &Room::typingChanged,
                 this, &ChatRoomWidget::typingChanged );
        connect( m_currentRoom, &Room::namesChanged,
                 this, &ChatRoomWidget::updateHeader );
        connect( m_currentRoom, &Room::topicChanged,
                 this, &ChatRoomWidget::updateHeader );
        connect( m_currentRoom, &Room::avatarChanged,
                 this, &ChatRoomWidget::updateHeader );
        connect( m_currentRoom, &Room::readMarkerMoved, this, [this] {
            const auto rm = m_currentRoom->readMarker();
            readMarkerOnScreen =
                rm != m_currentRoom->timelineEdge() &&
                std::lower_bound( indicesOnScreen.begin(), indicesOnScreen.end(),
                                 rm->index() ) != indicesOnScreen.end();
            reStartShownTimer();
            emit readMarkerMoved();
        });
        connect(m_currentRoom->connection(), &Connection::loggedOut, this, [=]
        {
            qWarning() << "Logged out, escaping the room";
            setRoom(nullptr);
        });
        connect(m_currentRoom->connection(), &Connection::joinedRoom,
                this, [=] (Room* room, Room* prev)
        {
            if (m_currentRoom == prev)
                setRoom(static_cast<QuaternionRoom*>(room));
        });

        m_currentRoom->setShown(true);
    } else
        m_imageProvider->setConnection(nullptr);
    updateHeader();
    typingChanged();
    m_messageModel->changeRoom( m_currentRoom );
    QObject* rootItem = m_quickView->rootObject();
    QMetaObject::invokeMethod(rootItem, "scrollToBottom");
}

void ChatRoomWidget::typingChanged()
{
    if (!m_currentRoom || m_currentRoom->usersTyping().isEmpty())
    {
        m_currentlyTyping->clear();
        return;
    }
    QStringList typingNames;
    for(auto user: m_currentRoom->usersTyping())
    {
        typingNames << m_currentRoom->roomMembername(user);
    }
    m_currentlyTyping->setText(tr("<i>Currently typing: %1</i>")
                               .arg( typingNames.join(", ") ) );
}

void ChatRoomWidget::updateHeader()
{
    if (m_currentRoom)
    {
        auto topic = m_currentRoom->topic();
        auto prettyTopic = topic.isEmpty() ?
                tr("(no topic)") : m_currentRoom->prettyPrint(topic);
        m_topicLabel->setText("<strong>" % m_currentRoom->name() %
                              "</strong><br />" % prettyTopic);
        auto avatarSize = m_topicLabel->heightForWidth(width());
        m_roomAvatar->setPixmap(m_currentRoom->avatar(avatarSize, avatarSize));
    }
    else
    {
        m_roomAvatar->clear();
        m_topicLabel->clear();
    }
}

bool ChatRoomWidget::checkAndRun(const QString& checkArg, const QString& pattern,
                                 std::function<void()> fn,
                                 const QString& errorMsg) const
{
    if (QRegularExpression(pattern).match(checkArg).hasMatch())
    {
        fn();
        return true;
    }

    emit showStatusMessage(errorMsg, 5000);
    return false;
}

bool ChatRoomWidget::checkAndRun1(const QString& args, const QString& pattern,
    std::function<void(QMatrixClient::Room*, QString)> fn1,
    const QString& errorMsg) const
{
    return checkAndRun(args, pattern,
                       std::bind(fn1, m_currentRoom, args), errorMsg);
}

bool ChatRoomWidget::checkAndRun2(const QString& args, const QString& pattern1,
    std::function<void(QMatrixClient::Room*, QString, QString)> fn2,
    const QString& errorMsg) const
{
    const auto nextSpacePos = args.indexOf(' ', 1);
    const auto arg1 = args.left(nextSpacePos);
    const auto arg2 = args.mid(nextSpacePos + 1).trimmed();
    return checkAndRun(arg1, pattern1,
                       std::bind(fn2, m_currentRoom, arg1, arg2), errorMsg);
}

bool ChatRoomWidget::doSendInput()
{
    QString text = m_chatEdit->toPlainText();
    if ( text.isEmpty() )
        return false;

    if (!text.startsWith('/'))
    {
        m_currentRoom->postMessage(text);
        return true;
    }
    if (text[1] == '/')
    {
        text.remove(0, 1);
        m_currentRoom->postMessage(text);
        return true;
    }

    // Process a command
    static const QRegularExpression re("^/([^ ]+)( (.*))?$");
    const auto matches = re.match(text);
    const auto command = matches.capturedRef(1);
    const auto args = matches.captured(3);

    static const auto ROOM_ID = QStringLiteral("^[#!][-0-9a-z._=]+:.+$");
    static const auto USER_ID = QStringLiteral("^@[-0-9a-z._=]+:.+$");

    using QMatrixClient::Room;
    // Commands available without a current room
    if (command == "join")
    {
        return checkAndRun(args, ROOM_ID,
            [=] { emit joinCommandEntered(args); },
            tr("/join argument doesn't look like a room ID or alias"));
    }
    // --- Add more roomless commands here
    if (!m_currentRoom)
    {
        emit showStatusMessage(
            tr("There's no such /command outside of room."
                   " Start with // to send this line literally"), 5000);
        return false;
    }
    // Commands available only in the room context
    if (command == "leave")
    {
        m_currentRoom->leaveRoom();
        return true;
    }
    if (command == "forget")
    {
        if (args.isEmpty())
        {
            emit showStatusMessage(
                tr("/forget should be followed by the room id,"
                   " even for the current room"));
            return false;
        }
        return checkAndRun(args, ROOM_ID,
            [=] { m_currentRoom->connection()->forgetRoom(args); },
            tr("/forget should be followed by the room id"));
    }
    if (command == "invite")
    {
        return checkAndRun1(args, USER_ID, &Room::inviteToRoom,
            tr("/invite argument doesn't look like a user ID"));
    }
    if (command == "kick")
    {
        return checkAndRun2(args, USER_ID, &Room::kickMember,
            tr("The first /kick argument doesn't look like a user ID"));
    }
    if (command == "ban")
    {
        return checkAndRun2(args, USER_ID, &Room::ban,
            tr("The first /ban argument doesn't look like a user ID"));
    }
    if (command == "unban")
    {
        return checkAndRun1(args, USER_ID, &Room::unban,
            tr("/unban argument doesn't look like a user ID"));
    }
    using MsgType = QMatrixClient::RoomMessageEvent::MsgType;
    if (command == "me")
    {
        m_currentRoom->postMessage(args, MsgType::Emote);
        return true;
    }
    if (command == "notice")
    {
        m_currentRoom->postMessage(args, MsgType::Notice);
        return true;
    }
    if (command == "shrug") // Peeked at Discord
    {
        m_currentRoom->postMessage("¯\\_(ツ)_/¯");
        return true;
    }
    if (command == "topic")
    {
        m_currentRoom->setTopic(args);
        return true;
    }
    if (command == "nick")
    {
        // Technically, it's legitimate to change the displayname outside of
        // any room; however, since Quaternion allows having several connections,
        // it needs to understand which connection to engage, and there's no good
        // way so far to determine the connection outside of a room.
        m_currentRoom->localUser()->rename(args);
        return true;
    }
    // --- Add more room commands here
    qDebug() << "Unknown command:" << command;
    emit showStatusMessage(
        tr("Unknown /command. Use // to send this line literally"), 5000);
    return false;
}

void ChatRoomWidget::sendInput()
{
    if (doSendInput())
        m_chatEdit->saveInput();
}

QStringList ChatRoomWidget::findCompletionMatches(const QString& pattern) const
{
    QStringList matches;
    if (m_currentRoom)
    {
        for(auto name: m_currentRoom->memberNames() )
        {
            if ( name.startsWith(pattern, Qt::CaseInsensitive) )
            {
                int ircSuffixPos = name.indexOf(" (IRC)");
                if ( ircSuffixPos != -1 )
                    name.truncate(ircSuffixPos);
                matches.append(name);
            }
        }
        std::sort(matches.begin(), matches.end(),
            [] (const QString& s1, const QString& s2)
                { return s1.localeAwareCompare(s2) < 0; });
        matches.removeDuplicates();
    }
    return matches;
}

void ChatRoomWidget::onMessageShownChanged(QString eventId, bool shown)
{
    if (!m_currentRoom)
        return;

    // A message can be auto-marked as read (as soon as the user is active), if:
    // 0. The read marker exists and is on the screen
    // 1. The message is shown on the screen now
    // 2. It's been the bottommost message on the screen for the last 1 second
    // 3. It's below the read marker

    const auto readMarker = m_currentRoom->readMarker();
    if (readMarker != m_currentRoom->timelineEdge() &&
            readMarker->event()->id() == eventId)
    {
        readMarkerOnScreen = shown;
        if (shown)
        {
            qDebug() << "Read marker is on-screen, at" << *readMarker;
            indexToMaybeRead = readMarker->index();
            reStartShownTimer();
        } else
        {
            qDebug() << "Read marker is off-screen";
            qDebug() << "Bottommost shown message index was" << indexToMaybeRead;
            maybeReadTimer.stop();
        }
    }

    const auto iter = m_currentRoom->findInTimeline(eventId);
    Q_ASSERT(iter != m_currentRoom->timelineEdge());
    const auto timelineIndex = iter->index();
    auto pos = std::lower_bound(indicesOnScreen.begin(), indicesOnScreen.end(),
                                timelineIndex);
    if (shown)
    {
        if (pos == indicesOnScreen.end() || *pos != timelineIndex)
        {
            indicesOnScreen.insert(pos, timelineIndex);
            if (timelineIndex == indicesOnScreen.back())
                reStartShownTimer();
        }
    } else
    {
        if (pos != indicesOnScreen.end() && *pos == timelineIndex)
            if (indicesOnScreen.erase(pos) == indicesOnScreen.end())
                reStartShownTimer();
    }
}

void ChatRoomWidget::reStartShownTimer()
{
    if (!readMarkerOnScreen || indicesOnScreen.empty() ||
            indexToMaybeRead >= indicesOnScreen.back())
        return;

    maybeReadTimer.start(1000, this);
    qDebug() << "Scheduled maybe-read message update:"
             << indexToMaybeRead << "->" << indicesOnScreen.back();
}

void ChatRoomWidget::timerEvent(QTimerEvent* qte)
{
    if (qte->timerId() != maybeReadTimer.timerId())
    {
        QWidget::timerEvent(qte);
        return;
    }
    maybeReadTimer.stop();
    // Only update the maybe-read message if we're tracking it
    if (readMarkerOnScreen && !indicesOnScreen.empty() &&
            indexToMaybeRead < indicesOnScreen.back())
    {
        qDebug() << "Maybe-read message update:" << indexToMaybeRead
                 << "->" << indicesOnScreen.back();
        indexToMaybeRead = indicesOnScreen.back();
        emit readMarkerCandidateMoved();
    }
}

void ChatRoomWidget::markShownAsRead()
{
    // FIXME: a case when a single message doesn't fit on the screen.
    if (m_currentRoom && readMarkerOnScreen)
    {
        const auto iter = m_currentRoom->findInTimeline(indicesOnScreen.back());
        Q_ASSERT( iter != m_currentRoom->timelineEdge() );
        m_currentRoom->markMessagesAsRead((*iter)->id());
    }
}

bool ChatRoomWidget::pendingMarkRead() const
{
    if (!readMarkerOnScreen || !m_currentRoom)
        return false;

    const auto rm = m_currentRoom->readMarker();
    return rm != m_currentRoom->timelineEdge() && rm->index() < indexToMaybeRead;
}
