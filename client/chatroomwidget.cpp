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

#include <QtCore/QDebug>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QLabel>

#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickView>
#include <QtQuick/QQuickItem>

#include "lib/user.h"
#include "lib/connection.h"
#include "lib/events/typingevent.h"
#include "models/messageeventmodel.h"
#include "imageprovider.h"
#include "chatedit.h"

ChatRoomWidget::ChatRoomWidget(QWidget* parent)
    : QWidget(parent)
    , m_currentRoom(nullptr)
    , m_currentConnection(nullptr)
    , readMarkerOnScreen(false)
{
    qmlRegisterType<QuaternionRoom>();
    m_messageModel = new MessageEventModel(this);

    m_topicLabel = new QLabel();
    m_topicLabel->setTextFormat(Qt::RichText);
    m_topicLabel->setWordWrap(true);
    m_topicLabel->setTextInteractionFlags(Qt::TextBrowserInteraction
                                          |Qt::TextSelectableByKeyboard);
    m_topicLabel->setOpenExternalLinks(true);

    auto topicSeparator = new QFrame();
    topicSeparator->setFrameShape(QFrame::HLine);

    m_quickView = new QQuickView();

    m_imageProvider = new ImageProvider(m_currentConnection);
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

    m_chatEdit = new ChatEdit(this);
    m_chatEdit->setPlaceholderText(tr("Send a message (unencrypted)..."));
    m_chatEdit->setAcceptRichText(false);
    connect( m_chatEdit, &KChatEdit::returnPressed, this, &ChatRoomWidget::sendInput );
    connect(m_chatEdit, &ChatEdit::proposedCompletion, this,
            [=](const QStringList& matches, int pos)
            {
                m_currentlyTyping->setText(
                    QStringLiteral("<i>Tab Completion (next: %1)</i>")
                    .arg( QStringList(matches.mid(pos, 5)).join(", ") ) );
            });
    connect(m_chatEdit, &ChatEdit::cancelledCompletion,
            this, &ChatRoomWidget::typingChanged);

    QVBoxLayout* layout = new QVBoxLayout();
    layout->addWidget(m_topicLabel);
    layout->addWidget(topicSeparator);
    layout->addWidget(container);
    layout->addWidget(m_currentlyTyping);
    layout->addWidget(m_chatEdit);
    setLayout(layout);
}

ChatRoomWidget::~ChatRoomWidget()
{
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

    readMarkerOnScreen = false;
    maybeReadTimer.stop();
    indicesOnScreen.clear();
    m_chatEdit->cancelCompletion();
    if( m_currentRoom )
    {
        m_currentRoom->setCachedInput( m_chatEdit->toPlainText() );
        m_currentRoom->disconnect( this );
        m_currentRoom->setShown(false);
        roomHistories.insert(m_currentRoom, m_chatEdit->history());
    }
    m_currentRoom = room;
    if( m_currentRoom )
    {
        m_chatEdit->setText( m_currentRoom->cachedInput() );
        m_chatEdit->setHistory(roomHistories.value(m_currentRoom));
        m_chatEdit->setFocus();
        m_chatEdit->moveCursor(QTextCursor::End);
        connect( m_currentRoom, &QMatrixClient::Room::typingChanged, this, &ChatRoomWidget::typingChanged );
        connect( m_currentRoom, &QMatrixClient::Room::topicChanged, this, &ChatRoomWidget::topicChanged );
        connect( m_currentRoom, &QMatrixClient::Room::readMarkerMoved, this, [this] {
            const auto rm = m_currentRoom->readMarker();
            readMarkerOnScreen =
                rm != m_currentRoom->timelineEdge() &&
                std::lower_bound( indicesOnScreen.begin(), indicesOnScreen.end(),
                                 rm->index() ) != indicesOnScreen.end();
            reStartShownTimer();
            emit readMarkerMoved();
        });
        m_currentRoom->setShown(true);
    }
    topicChanged();
    typingChanged();
    m_messageModel->changeRoom( m_currentRoom );
    //m_messageView->scrollToBottom();
    QObject* rootItem = m_quickView->rootObject();
    QMetaObject::invokeMethod(rootItem, "scrollToBottom");
}

void ChatRoomWidget::setConnection(QMatrixClient::Connection* connection)
{
    setRoom(nullptr);
    m_currentConnection = connection;
    m_imageProvider->setConnection(connection);
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
    m_currentlyTyping->setText(QStringLiteral("<i>Currently typing: %1</i>")
                               .arg( typingNames.join(", ") ) );
}

void ChatRoomWidget::topicChanged()
{
    if (m_currentRoom)
        m_topicLabel->setText(m_currentRoom->prettyTopic());
    else
        m_topicLabel->clear();
}

void ChatRoomWidget::sendInput()
{
    qDebug() << "sendLine";
    if( !m_currentConnection )
        return;
    QString text = m_chatEdit->toPlainText();
    if ( text.isEmpty() )
        return;

    // Commands available without current room
    if( text.startsWith("/join") )
    {
        const QString roomName = text.section(' ', 1, 1, QString::SectionSkipEmpty);
        emit joinCommandEntered(roomName);
    }
    else // Commands available only in the room context
        if (m_currentRoom)
        {
            if( text.startsWith("/leave") )
            {
                m_currentConnection->leaveRoom( m_currentRoom );
            }
            else if( text.startsWith("/me") )
            {
                text.remove(0, 3);
                m_currentRoom->postMessage("m.emote", text);
            }
            else if( text.startsWith("//") )
            {
                text.remove(0, 1);
                m_currentRoom->postMessage("m.text", text);
            }
            else if( text.startsWith('/') )
            {
                emit showStatusMessage( "Unknown command. Use // to send this line literally", 5000);
                return;
            } else
                m_currentRoom->postMessage("m.text", text);
        }
    m_chatEdit->saveInput();
}

QStringList ChatRoomWidget::findCompletionMatches(const QString& pattern) const
{
    QStringList matches;
    for(auto user: m_currentRoom->users() )
    {
        QString name = m_currentRoom->roomMembername(user);
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
    // FIXME: This doesn't cover a case when a single message doesn't fit
    // on the screen.
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
