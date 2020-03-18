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

#include "userlistmodel.h"

#include <QElapsedTimer>
#include <QtCore/QDebug>
#include <QtGui/QPixmap>
#include <QtGui/QPalette>
#include <QtWidgets/QApplication>

#include <connection.h>
#include <room.h>
#include <user.h>


UserListModel::UserListModel(QObject* parent)
    : QAbstractListModel(parent)
    , m_currentRoom(nullptr)
{ }

UserListModel::~UserListModel() = default;

void UserListModel::setRoom(Quotient::Room* room)
{
    if (m_currentRoom == room)
        return;

    using namespace Quotient;
    beginResetModel();
    if( m_currentRoom )
    {
        m_currentRoom->connection()->disconnect( this );
        m_currentRoom->disconnect( this );
        for( User* user: m_users )
            user->disconnect( this );
        m_users.clear();
    }
    m_currentRoom = room;
    if( m_currentRoom )
    {
        connect( m_currentRoom, &Room::userAdded, this, &UserListModel::userAdded );
        connect( m_currentRoom, &Room::userRemoved, this, &UserListModel::userRemoved );
        connect( m_currentRoom, &Room::memberAboutToRename, this, &UserListModel::userRemoved );
        connect( m_currentRoom, &Room::memberRenamed, this, &UserListModel::userAdded );
        connect( m_currentRoom, &Room::memberListChanged, this, &UserListModel::membersChanged );

        filter("");

        for( User* user: m_users )
        {
            connect( user, &User::avatarChanged, this, &UserListModel::avatarChanged );
        }
        connect( m_currentRoom->connection(), &Connection::loggedOut,
                 this, [=] { setRoom(nullptr); } );
        qDebug() << m_users.count() << "user(s) in the room";
    }
    endResetModel();
}

Quotient::User* UserListModel::userAt(QModelIndex index)
{
    if (index.row() < 0 || index.row() >= m_users.size())
        return nullptr;
    return m_users.at(index.row());
}

QVariant UserListModel::data(const QModelIndex& index, int role) const
{
    if( !index.isValid() )
        return QVariant();

    if( index.row() >= m_users.count() )
    {
        qDebug() << "UserListModel, something's wrong: index.row() >= m_users.count()";
        return QVariant();
    }
    auto user = m_users.at(index.row());
    if( role == Qt::DisplayRole )
    {
        return user->displayname(m_currentRoom);
    }
    if( role == Qt::DecorationRole )
    {
        return user->avatar(25,25, m_currentRoom);
    }

    if (role == Qt::ToolTipRole)
    {
        auto tooltip = QStringLiteral("<b>%1</b><br>%2")
                .arg(user->name(m_currentRoom).toHtmlEscaped(), user->id());
        if (!user->bridged().isEmpty())
            tooltip += "<br>" + tr("Bridged from: %1").arg(user->bridged());
        return tooltip;
    }

    if (role == Qt::ForegroundRole)
    {
        // FIXME: boilerplate with TimelineItem.qml:57
        return QColor::fromHslF(user->hueF(),
                                1 - QApplication::palette().color(QPalette::Window).saturationF(),
                                -0.7 * QApplication::palette().color(QPalette::Window).lightnessF() + 0.9,
                                QApplication::palette().color(QPalette::ButtonText).alphaF()
                                );
    }

    return QVariant();
}

int UserListModel::rowCount(const QModelIndex& parent) const
{
    if( parent.isValid() )
        return 0;

    return m_users.count();
}

void UserListModel::userAdded(Quotient::User* user)
{
    auto pos = findUserPos(user);
    if (pos != m_users.size() && m_users[pos] == user)
    {
        qWarning() << "Trying to add the user" << user->id()
                   << "but it's already in the user list";
        return;
    }
    beginInsertRows(QModelIndex(), pos, pos);
    m_users.insert(pos, user);
    endInsertRows();
    connect( user, &Quotient::User::avatarChanged, this, &UserListModel::avatarChanged );
}

void UserListModel::userRemoved(Quotient::User* user)
{
    auto pos = findUserPos(user);
    if (pos == m_users.size())
    {
        qWarning() << "Trying to remove a room member not in the user list:"
                   << user->id();
        return;
    }

    beginRemoveRows(QModelIndex(), pos, pos);
    m_users.removeAt(pos);
    endRemoveRows();
    user->disconnect(this);
}

void UserListModel::filter(const QString& filterString)
{
    if (m_currentRoom == nullptr)
        return;

    QElapsedTimer et; et.start();

    beginResetModel();
    m_users.clear();
    const auto all = m_currentRoom->users();
    std::remove_copy_if(all.begin(), all.end(), std::back_inserter(m_users),
        [&](User* u) {
            return !(u->rawName(m_currentRoom).contains(filterString) ||
                     u->id().contains(filterString));
        });
    std::sort(m_users.begin(), m_users.end(), m_currentRoom->memberSorter());
    endResetModel();

    qDebug() << "Filtering" << m_users.size() << "user(s) in"
                << m_currentRoom->displayName() << "took" << et;
}

void UserListModel::refresh(Quotient::User* user, QVector<int> roles)
{
    auto pos = findUserPos(user);
    if ( pos != m_users.size() )
        emit dataChanged(index(pos), index(pos), roles);
    else
        qWarning() << "Trying to access a room member not in the user list";
}

void UserListModel::avatarChanged(Quotient::User* user,
                                  const Quotient::Room* context)
{
    if (context == m_currentRoom)
        refresh(user, {Qt::DecorationRole});
}

int UserListModel::findUserPos(User* user) const
{
    return findUserPos(m_currentRoom->roomMembername(user));
}

int UserListModel::findUserPos(const QString& username) const
{
    return m_currentRoom->memberSorter().lowerBoundIndex(m_users, username);
}
