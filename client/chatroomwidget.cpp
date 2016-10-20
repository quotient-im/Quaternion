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
#include <QtCore/QTimer>
#include <QtWidgets/QListView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QLabel>

#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickView>
#include <QtQuick/QQuickItem>

#include "lib/room.h"
#include "lib/user.h"
#include "lib/connection.h"
#include "lib/logmessage.h"
#include "lib/jobs/postmessagejob.h"
#include "lib/events/event.h"
#include "lib/events/typingevent.h"
#include "models/messageeventmodel.h"
#include "quaternionroom.h"
#include "imageprovider.h"

class ChatEdit : public QLineEdit
{
    public:
        ChatEdit(ChatRoomWidget* c);
    protected:
        bool event(QEvent *event);
    private:
        ChatRoomWidget* m_chatRoomWidget;
};

ChatEdit::ChatEdit(ChatRoomWidget* c): m_chatRoomWidget(c) {};

bool ChatEdit::event(QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Tab) {
            m_chatRoomWidget->triggerCompletion();
            return true;
        } else
            m_chatRoomWidget->cancelCompletion();
    }
    return QLineEdit::event(event);
}

ChatRoomWidget::ChatRoomWidget(QWidget* parent)
    : QWidget(parent)
{
    m_messageModel = new MessageEventModel(this);
    m_currentRoom = nullptr;
    m_currentConnection = nullptr;
    m_completing = false;

    //m_messageView = new QListView();
    //m_messageView->setModel(m_messageModel);

    m_quickView = new QQuickView();

    m_imageProvider = new ImageProvider(m_currentConnection);
    m_quickView->engine()->addImageProvider("mtx", m_imageProvider);

    QWidget* container = QWidget::createWindowContainer(m_quickView, this);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QQmlContext* ctxt = m_quickView->rootContext();
    ctxt->setContextProperty("messageModel", m_messageModel);
    ctxt->setContextProperty("debug", QVariant(false));
    m_quickView->setSource(QUrl("qrc:///qml/chat.qml"));
    m_quickView->setResizeMode(QQuickView::SizeRootObjectToView);

    QObject* rootItem = m_quickView->rootObject();
    connect( rootItem, SIGNAL(getPreviousContent()), this, SLOT(getPreviousContent()) );


    m_chatEdit = new ChatEdit(this);
    connect( m_chatEdit, &QLineEdit::returnPressed, this, &ChatRoomWidget::sendLine );

    m_currentlyTyping = new QLabel();
    m_topicLabel = new QLabel();
    m_topicLabel->setWordWrap(true);
    m_topicLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);


    QVBoxLayout* layout = new QVBoxLayout();
    layout->addWidget(m_topicLabel);
    layout->addWidget(container);
    layout->addWidget(m_currentlyTyping);
    layout->addWidget(m_chatEdit);
    setLayout(layout);
}

ChatRoomWidget::~ChatRoomWidget()
{
}

void ChatRoomWidget::lookAtRoom()
{
    if ( m_currentRoom )
        m_currentRoom->lookAt();
}

void ChatRoomWidget::enableDebug()
{
    QQmlContext* ctxt = m_quickView->rootContext();
    ctxt->setContextProperty("debug", true);
}

void ChatRoomWidget::setRoom(QuaternionRoom* room)
{
    if( m_currentRoom )
    {
        m_currentRoom->setCachedInput( m_chatEdit->displayText() );
        m_currentRoom->disconnect( this );
        m_currentRoom->setShown(false);
        if ( m_completing )
            cancelCompletion();
    }
    m_currentRoom = room;
    if( m_currentRoom )
    {
        m_chatEdit->setText( m_currentRoom->cachedInput() );
        connect( m_currentRoom, &QMatrixClient::Room::typingChanged, this, &ChatRoomWidget::typingChanged );
        connect( m_currentRoom, &QMatrixClient::Room::topicChanged, this, &ChatRoomWidget::topicChanged );
        connect( m_currentRoom, &QMatrixClient::Room::lastReadEventChanged, this, &ChatRoomWidget::updateReadMarker );
        m_currentRoom->setShown(true);
        topicChanged();
        typingChanged();
        updateReadMarker(m_currentConnection->user());
    } else {
        m_topicLabel->clear();
        m_currentlyTyping->clear();
    }
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
    m_messageModel->setConnection(connection);
}

void ChatRoomWidget::typingChanged()
{
    QList<QMatrixClient::User*> typing = m_currentRoom->usersTyping();
    if( typing.isEmpty() )
    {
        m_currentlyTyping->clear();
        return;
    }
    QStringList typingNames;
    for( QMatrixClient::User* user: typing )
    {
        typingNames << m_currentRoom->roomMembername(user);
    }
    m_currentlyTyping->setText( QString("<i>Currently typing: %1</i>").arg( typingNames.join(", ") ) );
}

void ChatRoomWidget::updateReadMarker(QMatrixClient::User* user)
{
    if (user == m_currentConnection->user())
        m_quickView->rootContext()
            ->setContextProperty("lastReadId", m_currentRoom->lastReadEvent(user));
}

void ChatRoomWidget::topicChanged()
{
    m_topicLabel->setText( m_currentRoom->topic() );
}

void ChatRoomWidget::getPreviousContent()
{
    if (m_currentRoom)
        m_currentRoom->getPreviousContent();
}

void ChatRoomWidget::sendLine()
{
    qDebug() << "sendLine";
    if( !m_currentConnection )
        return;
    QString text = m_chatEdit->displayText();
    if ( text.isEmpty() )
        return;

    // Commands available without current room
    if( text.startsWith("/join") )
    {
        QString roomName = text.section(' ', 1, 1, QString::SectionSkipEmpty);
        if( !roomName.isEmpty() )
            m_currentConnection->joinRoom( roomName );
        else
        {
            qDebug() << "No arguments for /join, going interactive";
            emit joinRoomNeedsInteraction();
        }
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
                m_currentConnection->postMessage(m_currentRoom, "m.emote", text);
            }
            else if( text.startsWith("//") )
            {
                text.remove(0, 1);
                m_currentConnection->postMessage(m_currentRoom, "m.text", text);
            }
            else if( text.startsWith("/") )
            {
                emit showStatusMessage( "Unknown command. Use // to send this line literally", 5000);
                return;
            } else
                m_currentConnection->postMessage(m_currentRoom, "m.text", text);
        }
    m_chatEdit->setText("");
}

void ChatRoomWidget::findCompletionMatches(const QString& pattern)
{
    for( QMatrixClient::User* user: m_currentRoom->users() )
    {
        QString name = m_currentRoom->roomMembername(user);
        if ( name.startsWith(pattern, Qt::CaseInsensitive) )
        {
            int ircSuffixPos = name.indexOf(" (IRC)");
            if ( ircSuffixPos != -1 )
                name.truncate(ircSuffixPos);
            m_completionList.append(name);
        }
    }
    m_completionList.sort(Qt::CaseInsensitive);
    m_completionList.removeDuplicates();
}

void ChatRoomWidget::cancelCompletion()
{
    m_completing = false;
    m_completionList.clear();
    if (m_currentConnection && m_currentRoom)
        typingChanged();
}

void ChatRoomWidget::triggerCompletion()
{
    if ( !m_completing && m_currentConnection && m_currentRoom )
    {
        startNewCompletion();
    }
    if ( m_completing )
    {
        QString inputText = m_chatEdit->text();
        m_chatEdit->setText( inputText.left(m_completionInsertStart)
            + m_completionList.at(m_completionListPosition)
            + inputText.right(inputText.length() - m_completionInsertStart - m_completionLength) );
        m_completionLength = m_completionList.at(m_completionListPosition).length();
        m_chatEdit->setCursorPosition( m_completionInsertStart + m_completionLength + m_completionCursorOffset );
        m_completionListPosition = (m_completionListPosition + 1) % m_completionList.length();
        m_currentlyTyping->setText( QString("<i>Tab Completion (next: %1)</i>").arg(
            QStringList(m_completionList.mid( m_completionListPosition, 5)).join(", ") ) );
    }
}

void ChatRoomWidget::startNewCompletion()
{
    QString inputText = m_chatEdit->text();
    int cursorPosition = m_chatEdit->cursorPosition();
    for ( m_completionInsertStart = cursorPosition; --m_completionInsertStart >= 0; )
    {
        if ( !(inputText.at(m_completionInsertStart).isLetterOrNumber() || inputText.at(m_completionInsertStart) == '@') )
            break;
    }
    ++m_completionInsertStart;
    m_completionLength = cursorPosition - m_completionInsertStart;
    findCompletionMatches(inputText.mid(m_completionInsertStart, m_completionLength));
    if ( !m_completionList.isEmpty() )
    {
        m_completionCursorOffset = 0;
        m_completionListPosition = 0;
        m_completing = true;
        m_completionLength = 0;
        if ( m_completionInsertStart == 0)
        {
            m_chatEdit->setText(inputText.left(m_completionInsertStart) + ": " + inputText.mid(cursorPosition));
            m_completionCursorOffset = 2;
        }
        else if ( inputText.mid(m_completionInsertStart - 2, 2) == ": ")
        {
            m_chatEdit->setText(inputText.left(m_completionInsertStart - 2) + ", : " + inputText.mid(cursorPosition));
            m_completionCursorOffset = 2;
        }
        else if ( inputText.mid(m_completionInsertStart - 1, 1) == ":")
        {
            m_chatEdit->setText(inputText.left(m_completionInsertStart - 1) + ", : " + inputText.mid(cursorPosition));
            ++m_completionInsertStart;
            m_completionCursorOffset = 2;
        }
        else
        {
            m_chatEdit->setText(inputText.left(m_completionInsertStart) + " " + inputText.mid(cursorPosition));
            m_completionCursorOffset = 1;
        }
    }
}
