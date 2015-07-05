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

#include "logmessagemodel.h"

#include <QtCore/QDebug>

#include "lib/room.h"
#include "lib/logmessage.h"

LogMessageModel::LogMessageModel(QObject* parent)
    : QAbstractListModel(parent)
{
    m_currentRoom = 0;
}

LogMessageModel::~LogMessageModel()
{
}

void LogMessageModel::changeRoom(QMatrixClient::Room* room)
{
    beginResetModel();
    if( m_currentRoom )
    {
        disconnect( m_currentRoom, &QMatrixClient::Room::newMessages, this, &LogMessageModel::newMessages );
    }
    m_currentRoom = room;
    if( room )
    {
        m_currentMessages = room->logMessages();
        connect( room, &QMatrixClient::Room::newMessages, this, &LogMessageModel::newMessages );
        qDebug() << "connected" << room;
    }
    else
    {
        m_currentMessages = QList<QMatrixClient::LogMessage*>();
    }
    endResetModel();
}

// QModelIndex LogMessageModel::index(int row, int column, const QModelIndex& parent) const
// {
//     if( parent.isValid() )
//         return QModelIndex();
//     if( row < 0 || row >= m_currentMessages.count() )
//         return QModelIndex();
//     return createIndex(row, column, m_currentMessages.at(row));
// }
//
// LogMessageModel::parent(const QModelIndex& index) const
// {
//     return QModelIndex();
// }

int LogMessageModel::rowCount(const QModelIndex& parent) const
{
    if( parent.isValid() )
        return 0;
    return m_currentMessages.count();
}

QVariant LogMessageModel::data(const QModelIndex& index, int role) const
{
    if( role != Qt::DisplayRole )
        return QVariant();
    if( index.row() < 0 || index.row() >= m_currentMessages.count() )
        return QVariant();
    QMatrixClient::LogMessage* msg = m_currentMessages.at(index.row());
    return msg->author() + ": " + msg->message();
}

void LogMessageModel::newMessages(QList< QMatrixClient::LogMessage* > messages)
{
    qDebug() << "Messages: " << messages;
    beginInsertRows(QModelIndex(), m_currentMessages.count(), m_currentMessages.count()+messages.count()-1);
    m_currentMessages.append(messages);
    endInsertRows();
}