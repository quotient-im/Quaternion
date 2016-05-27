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

#ifndef CHATROOMWIDGET_H
#define CHATROOMWIDGET_H

#include <QtWidgets/QWidget>

#include <QtQuick/QQuickView>

namespace QMatrixClient
{
    class Room;
    class Connection;
    class Event;
}
class MessageEventModel;
class QuaternionRoom;
class ImageProvider;
class QListView;
class QLineEdit;
class QLabel;

class ChatRoomWidget: public QWidget
{
        Q_OBJECT
    public:
        ChatRoomWidget(QWidget* parent = nullptr);
        virtual ~ChatRoomWidget();

        void enableDebug();

    public slots:
        void setRoom(QMatrixClient::Room* room);
        void setConnection(QMatrixClient::Connection* connection);
        void topicChanged();
        void typingChanged();
        void getPreviousContent();

    private slots:
        void sendLine();

    private:
        MessageEventModel* m_messageModel;
        QuaternionRoom* m_currentRoom;
        QMatrixClient::Connection* m_currentConnection;

        //QListView* m_messageView;
        QQuickView* m_quickView;
        ImageProvider* m_imageProvider;
        QLineEdit* m_chatEdit;
        QLabel* m_currentlyTyping;
        QLabel* m_topicLabel;
};

#endif // CHATROOMWIDGET_H
