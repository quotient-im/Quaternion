/******************************************************************************
 * Copyright (C) 2015 Felix Rohrbach <kde@fxrh.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "chatroomwidget.h"

#include <QtCore/QDebug>
#include <QtWidgets/QListView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QLabel>

#include "lib/room.h"
#include "lib/connection.h"
#include "lib/logmessage.h"
#include "lib/jobs/postmessagejob.h"
#include "lib/events/event.h"
#include "lib/events/typingevent.h"
#include "models/messageeventmodel.h"

ChatRoomWidget::ChatRoomWidget(QWidget* parent)
{
    m_messageModel = new MessageEventModel(this);
    m_currentRoom = 0;
    m_currentConnection = 0;

    m_messageView = new QListView();
    m_messageView->setModel(m_messageModel);

    m_chatEdit = new QLineEdit();
    connect( m_chatEdit, &QLineEdit::returnPressed, this, &ChatRoomWidget::sendLine );

    m_currentlyTyping = new QLabel();
    m_topicLabel = new QLabel();

    QVBoxLayout* layout = new QVBoxLayout();
    layout->addWidget(m_topicLabel);
    layout->addWidget(m_messageView);
    layout->addWidget(m_currentlyTyping);
    layout->addWidget(m_chatEdit);
    setLayout(layout);
}

ChatRoomWidget::~ChatRoomWidget()
{
}

void ChatRoomWidget::setRoom(QMatrixClient::Room* room)
{
    m_messageModel->changeRoom( room );
    if( m_currentRoom )
    {
        disconnect( m_currentRoom, &QMatrixClient::Room::newMessage, this, &ChatRoomWidget::newEvent );
        disconnect( m_currentRoom, &QMatrixClient::Room::topicChanged, this, &ChatRoomWidget::topicChanged );
    }
    m_currentRoom = room;
    if( m_currentRoom )
    {
        connect( m_currentRoom, &QMatrixClient::Room::newMessage, this, &ChatRoomWidget::newEvent );
        connect( m_currentRoom, &QMatrixClient::Room::topicChanged, this, &ChatRoomWidget::topicChanged );
        topicChanged();
    }
    m_messageView->scrollToBottom();
}

void ChatRoomWidget::setConnection(QMatrixClient::Connection* connection)
{
    m_currentConnection = connection;
}

void ChatRoomWidget::newEvent(QMatrixClient::Event* event)
{
    if( event->type() == QMatrixClient::EventType::Typing )
    {
        QMatrixClient::TypingEvent* e = static_cast<QMatrixClient::TypingEvent*>(event);
        if( e->users().count() > 0 )
            m_currentlyTyping->setText( QString("<i>Currently typing: %1</i>").arg( e->users().join(", ") ) );
        else
            m_currentlyTyping->setText("");
    }
}

void ChatRoomWidget::topicChanged()
{
    m_topicLabel->setText( m_currentRoom->topic() );
}

void ChatRoomWidget::sendLine()
{
    qDebug() << "sendLine";
    if( !m_currentRoom || !m_currentConnection )
        return;
    QString text = m_chatEdit->displayText();
    if( text.startsWith("/join") )
    {
        QStringList splitted = text.split(' ');
        if( splitted.count() > 1 )
            m_currentConnection->joinRoom( splitted[1] );
        else
            qDebug() << "No arguments for join";
    }
    else if( text.startsWith("/leave") )
    {
        m_currentConnection->leaveRoom( m_currentRoom );
    }
    else
    {
        m_currentConnection->postMessage(m_currentRoom, "m.text", m_chatEdit->displayText());
    }
    m_chatEdit->setText("");
}