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
}
class LogMessageModel;
class QListView;

class ChatRoomWidget: public QWidget
{
        Q_OBJECT
    public:
        ChatRoomWidget(QWidget* parent=0);
        virtual ~ChatRoomWidget();

    public slots:
        void setRoom(QMatrixClient::Room* room);

    private:
        LogMessageModel* m_messageModel;
        QMatrixClient::Room* m_currentRoom;

        QListView* m_messageView;
};

#endif // CHATROOMWIDGET_H