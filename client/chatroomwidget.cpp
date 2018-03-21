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
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QApplication>

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
    , m_messageModel(new MessageEventModel(this))
    , m_currentRoom(nullptr)
    , readMarkerOnScreen(false)
{
    {
        using namespace QMatrixClient;
        qmlRegisterUncreatableType<QuaternionRoom>("QMatrixClient", 1, 0, "Room",
            "Room objects can only be created by libqmatrixclient");
        qmlRegisterUncreatableType<User>("QMatrixClient", 1, 0, "User",
            "User objects can only be created by libqmatrixclient");
        qmlRegisterType<Settings>("QMatrixClient", 1, 0, "Settings");
    }

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
    m_quickView->setSource(QUrl("qrc:///qml/Timeline.qml"));
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
                    tr("<i>Next completion: %1</i>")
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
        roomHistories.insert(m_currentRoom, m_chatEdit->history());
        m_currentRoom->connection()->disconnect(this);
        m_currentRoom->disconnect( this );
    }
    readMarkerOnScreen = false;
    maybeReadTimer.stop();
    indicesOnScreen.clear();
    m_chatEdit->cancelCompletion();

    m_currentRoom = room;
    m_quickView->rootContext()->setContextProperty("room", room);
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
        connect(m_currentRoom->connection(), &Connection::loggedOut,
                this, [this]
        {
            qWarning() << "Logged out, escaping the room";
            setRoom(nullptr);
        });
        connect(m_currentRoom->connection(), &Connection::joinedRoom,
                this, [this] (Room* r, Room* prev)
        {
            if (m_currentRoom == prev)
                setRoom(static_cast<QuaternionRoom*>(r));
        });
        connect(m_currentRoom->connection(), &Connection::aboutToDeleteRoom,
                this, [this] (Room* r)
        {
            if (m_currentRoom == r)
                setRoom(nullptr);
        });
    } else
        m_imageProvider->setConnection(nullptr);
    updateHeader();
    typingChanged();
    m_messageModel->changeRoom( m_currentRoom );
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
        m_roomAvatar->setPixmap(
                QPixmap::fromImage(m_currentRoom->avatar(avatarSize)));
    }
    else
    {
        m_roomAvatar->clear();
        m_topicLabel->clear();
    }
}

void ChatRoomWidget::insertMention(QString author)
{
    m_chatEdit->insertMention(author);
}

void ChatRoomWidget::focusInput()
{
    m_chatEdit->setFocus();
}

/**
 * \brief Split the string into no more than specified numbers of parts
 * The function takes \p s and splits it into at most \p maxParts parts
 * using \p sep for the separator. Empty parts are skipped. If there are more
 * than \p maxParts parts in the string, the last returned part includes
 * the remainder of the string.
 * \return the vector of references to the original string, one reference for
 * each part.
 */
QVector<QStringRef> lazySplitRef(const QString& s, QChar sep, int maxParts)
{
    QVector<QStringRef> parts;
    int pos = 0, nextPos = 0;
    for (; maxParts > 1 && (nextPos = s.indexOf(sep, pos)) > -1; --maxParts)
    {
        parts.push_back({&s, pos, nextPos - pos});
        while (s[++nextPos] == sep)
            ;
        pos = nextPos;
    }
    parts.push_back(s.midRef(pos));
    return parts;
}

QString ChatRoomWidget::doSendInput()
{
    QString text = m_chatEdit->toPlainText();
    if ( text.isEmpty() )
        return tr("There's nothing to send");

    if (!text.startsWith('/'))
    {
        m_currentRoom->postMessage(text);
        return {};
    }
    if (text[1] == '/')
    {
        text.remove(0, 1);
        m_currentRoom->postMessage(text);
        return {};
    }

    static const auto ReFlags =
            QRegularExpression::DotMatchesEverythingOption|
            QRegularExpression::OptimizeOnFirstUsageOption;

    static const QRegularExpression
            CommandRe { "^/([^ ]+)( +(.*))?\\s*$", ReFlags },
            RoomIdRE { "^[#!][-0-9a-z._=]+:.+$", ReFlags },
            UserIdRE { "^@[-0-9a-z._=]+:.+$", ReFlags };

    // Process a command
    const auto matches = CommandRe.match(text);
    const auto command = matches.capturedRef(1);
    const auto argString = matches.captured(3);

    // Commands available without a current room
    if (command == "join")
    {
        if (!argString.contains(RoomIdRE))
            return tr("/join argument doesn't look like a room ID or alias");
        emit joinCommandEntered(argString);
    }
    if (command == "quit")
    {
        qApp->closeAllWindows();
        return {};
    }
    // --- Add more roomless commands here
    if (!m_currentRoom)
        return tr("There's no such /command outside of room."
                  " Start with // to send this line literally");

    // Commands available only in the room context
    using QMatrixClient::Room;
    if (command == "leave" || command == "part")
    {
        if (!argString.isEmpty())
            return tr("Sending a farewell message is not supported yet."
                      " If you intended to leave another room, switch to it"
                      " and type /leave there.");

        m_currentRoom->leaveRoom();
        return {};
    }
    if (command == "forget")
    {
        if (argString.isEmpty())
            return tr("/forget must be followed by the room id/alias,"
                      " even for the current room");
        if (!argString.contains(RoomIdRE))
            return tr("%1 doesn't look like a room id or alias").arg(argString);

        // Forget the specified room using the current room's connection
        m_currentRoom->connection()->forgetRoom(argString);
        return {};
    }
    if (command == "invite")
    {
        if (argString.isEmpty())
            return tr("/invite <memberId>");
        if (!argString.contains(UserIdRE))
            return tr("%1 doesn't look like a user ID").arg(argString);

        m_currentRoom->inviteToRoom(argString);
        return {};
    }
    if (command == "kick" || command == "ban")
    {
        const auto args = lazySplitRef(argString, ' ', 2);
        if (args.front().isEmpty())
            return tr("/%1 <memberId> <reason>").arg(command.toString());
        if (!UserIdRE.match(args.front()).hasMatch())
            return tr("%1 doesn't look like a user id")
                    .arg(args.front().toString());

        auto* user = m_currentRoom->user(args.front().toString());
        if (m_currentRoom->memberJoinState(user)
                != QMatrixClient::JoinState::Join)
            return tr("%1 is not a member of this room")
                    .arg(user->fullName(m_currentRoom));

        if (command == "ban")
            m_currentRoom->ban(user->id(), args.back().toString());
        else
            m_currentRoom->kickMember(user->id(), args.back().toString());
        return {};
    }
    if (command == "unban")
    {
        if (argString.isEmpty())
            return tr("/unban <memberId>");
        if (!argString.contains(UserIdRE))
            return tr("/unban argument doesn't look like a user ID");

        m_currentRoom->unban(argString);
        return {};
    }
    using MsgType = QMatrixClient::RoomMessageEvent::MsgType;
    if (command == "me")
    {
        if (argString.isEmpty())
            return tr("/me needs an argument");
        m_currentRoom->postMessage(argString, MsgType::Emote);
        return {};
    }
    if (command == "notice")
    {
        if (argString.isEmpty())
            return tr("/notice needs an argument");
        m_currentRoom->postMessage(argString, MsgType::Notice);
        return {};
    }
    if (command == "shrug") // Peeked at Discord
    {
        m_currentRoom->postMessage("¯\\_(ツ)_/¯");
        return {};
    }
    if (command == "topic")
    {
        m_currentRoom->setTopic(argString);
        return {};
    }
    if (command == "nick")
    {
        m_currentRoom->localUser()->rename(argString);
        return {};
    }
    if (command == "roomnick")
    {
        m_currentRoom->localUser()->rename(argString, m_currentRoom);
        return {};
    }
    // --- Add more room commands here
    qDebug() << "Unknown command:" << command;
    return tr("Unknown /command. Use // to send this line literally");
}

void ChatRoomWidget::sendInput()
{
    auto result = doSendInput();
    if (result.isEmpty())
        m_chatEdit->saveInput();
    else
        emit showStatusMessage(result, 5000);
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

void ChatRoomWidget::saveFileAs(QString eventId)
{
    if (!m_currentRoom)
    {
        qWarning()
            << "ChatRoomWidget::saveFileAs without an active room ignored";
        return;
    }
    auto fileName = QFileDialog::getSaveFileName(
                this, tr("Save file as"),
                m_currentRoom->fileNameToDownload(eventId));
    if (!fileName.isEmpty())
        m_currentRoom->downloadFile(eventId, QUrl::fromLocalFile(fileName));
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
    if (iter == m_currentRoom->timelineEdge())
    {
        qCritical() <<  "Event" << eventId << "is not in the timeline";
        Q_ASSERT(false);
        return;
    }
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
