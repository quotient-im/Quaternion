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
#include <QtCore/QTimer>
#include <QtWidgets/QListView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QLabel>

#include <QtQml/QQmlContext>
#include <QtQuick/QQuickItem>

#include "lib/room.h"
#include "lib/user.h"
#include "lib/connection.h"
#include "lib/logmessage.h"
#include "lib/jobs/postmessagejob.h"
#include "lib/events/event.h"
#include "lib/events/typingevent.h"
#include "models/messageeventmodel.h"

ChatRoomWidget::ChatRoomWidget(QWidget* parent)
{
    m_messageModel = new MessageEventModel(this);
    m_currentRoom = nullptr;
    m_currentConnection = nullptr;

    //m_messageView = new QListView();
    //m_messageView->setModel(m_messageModel);

    m_quickView = new QQuickView();
    QWidget* container = QWidget::createWindowContainer(m_quickView, this);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QQmlContext* ctxt = m_quickView->rootContext();
    ctxt->setContextProperty("messageModel", m_messageModel);
    m_quickView->setSource(QUrl("qrc:///qml/chat.qml"));
    m_quickView->setResizeMode(QQuickView::SizeRootObjectToView);

    QObject* rootItem = m_quickView->rootObject();
    connect( rootItem, SIGNAL(getPreviousContent()), this, SLOT(getPreviousContent()) );


    m_chatEdit = new QLineEdit();
    connect( m_chatEdit, &QLineEdit::returnPressed, this, &ChatRoomWidget::sendLine );

    m_currentlyTyping = new QLabel();
    m_topicLabel = new QLabel();

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

void ChatRoomWidget::setRoom(QMatrixClient::Room* room)
{
    if( m_currentRoom )
    {
        disconnect( m_currentRoom, &QMatrixClient::Room::typingChanged, this, &ChatRoomWidget::typingChanged );
        disconnect( m_currentRoom, &QMatrixClient::Room::topicChanged, this, &ChatRoomWidget::topicChanged );
    }
    m_currentRoom = room;
    if( m_currentRoom )
    {
        connect( m_currentRoom, &QMatrixClient::Room::typingChanged, this, &ChatRoomWidget::typingChanged );
        connect( m_currentRoom, &QMatrixClient::Room::topicChanged, this, &ChatRoomWidget::topicChanged );
        topicChanged();
    }
    m_messageModel->changeRoom( room );
    //m_messageView->scrollToBottom();
    QObject* rootItem = m_quickView->rootObject();
    QMetaObject::invokeMethod(rootItem, "scrollToBottom");
}

void ChatRoomWidget::setConnection(QMatrixClient::Connection* connection)
{
    m_currentConnection = connection;
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
        typingNames << user->displayname();
    }
    m_currentlyTyping->setText( QString("<i>Currently typing: %1</i>").arg( typingNames.join(", ") ) );
}

void ChatRoomWidget::topicChanged()
{
    m_topicLabel->setText( m_currentRoom->topic() );
}

void ChatRoomWidget::getPreviousContent()
{
    m_currentRoom->getPreviousContent();
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
