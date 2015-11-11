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

#ifndef CHATROOMWIDGET_H
#define CHATROOMWIDGET_H

#include <QtWidgets/QWidget>

namespace QMatrixClient
{
    class Room;
    class Connection;
    class Event;
}
class MessageEventModel;
class QListView;
class QLineEdit;
class QLabel;

class ChatRoomWidget: public QWidget
{
        Q_OBJECT
    public:
        ChatRoomWidget(QWidget* parent=0);
        virtual ~ChatRoomWidget();

    public slots:
        void setRoom(QMatrixClient::Room* room);
        void setConnection(QMatrixClient::Connection* connection);
        void newEvent(QMatrixClient::Event* event);
        void topicChanged();

    private slots:
        void sendLine();

    private:
        MessageEventModel* m_messageModel;
        QMatrixClient::Room* m_currentRoom;
        QMatrixClient::Connection* m_currentConnection;

        QListView* m_messageView;
        QLineEdit* m_chatEdit;
        QLabel* m_currentlyTyping;
        QLabel* m_topicLabel;
};

#endif // CHATROOMWIDGET_H