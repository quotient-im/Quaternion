/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2015 Felix Rohrbach <kde@fxrh.de>                        *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#include "userlistmodel.h"

#include "../logging_categories.h"

#include <QtCore/QDebug>
#include <QtGui/QPixmap>
#include <QtGui/QPalette>
#include <QtGui/QFontMetrics>
// Injecting the dependency on a view is not so nice; but the way the model
// provides avatar decorations depends on the delegate size
#include <QtWidgets/QAbstractItemView>

#include <Quotient/connection.h>
#include <Quotient/ranges_extras.h>
#include <Quotient/room.h>
#include <Quotient/user.h>

UserListModel::UserListModel(QAbstractItemView* parent)
    : QAbstractListModel(parent), m_currentRoom(nullptr)
{ }

void UserListModel::setRoom(Quotient::Room* room)
{
    if (m_currentRoom == room)
        return;

    using namespace Quotient;
    beginResetModel();
    if (m_currentRoom) {
        m_currentRoom->connection()->disconnect(this);
        m_currentRoom->disconnect(this);
        m_memberIds.clear();
    }
    m_currentRoom = room;
    if (m_currentRoom) {
        connect(m_currentRoom, &Room::memberJoined, this, &UserListModel::userAdded);
        connect(m_currentRoom, &Room::memberLeft, this, &UserListModel::userRemoved);
        connect(m_currentRoom, &Room::memberNameAboutToUpdate, this, &UserListModel::userRemoved);
        connect(m_currentRoom, &Room::memberNameUpdated, this, &UserListModel::userAdded);
        connect(m_currentRoom, &Room::memberListChanged, this, &UserListModel::membersChanged);
        connect(m_currentRoom, &Room::memberAvatarUpdated, this, &UserListModel::avatarChanged);
        connect(m_currentRoom->connection(), &Connection::loggedOut, this,
                [this] { setRoom(nullptr); });

        filter({});
        qCDebug(MODELS) << m_memberIds.count() << "member(s) in the room";
    }
    endResetModel();
}

Quotient::RoomMember UserListModel::userAt(QModelIndex index) const
{
    if (index.row() < 0 || index.row() >= m_memberIds.size())
        return {};
    return m_currentRoom->member(m_memberIds.at(index.row()));
}

QVariant UserListModel::data(const QModelIndex& index, int role) const
{
    if( !index.isValid() )
        return QVariant();

    if( index.row() >= m_memberIds.count() )
    {
        qCWarning(MODELS) << "UserListModel, something's wrong: index.row() >= "
                             "m_users.count()";
        return QVariant();
    }
    auto m = userAt(index);
    if( role == Qt::DisplayRole )
    {
        return m.displayName();
    }
    const auto* view = static_cast<const QAbstractItemView*>(parent());
    if (role == Qt::DecorationRole) {
        // Convert avatar image to QIcon
        const auto dpi = view->devicePixelRatioF();
        if (auto av = m.avatar(static_cast<int>(view->iconSize().height() * dpi), [] {});
            !av.isNull()) {
            av.setDevicePixelRatio(dpi);
            return QIcon(QPixmap::fromImage(av));
        }
        // TODO: Show a different fallback icon for invited users
        return QIcon::fromTheme("user-available",
                                QIcon(":/irc-channel-joined"));
    }

    if (role == Qt::ToolTipRole)
    {
        auto tooltip =
            QStringLiteral("<b>%1</b><br>%2").arg(m.name().toHtmlEscaped(), m.id().toHtmlEscaped());
        // TODO: Find a new way to determine that the user is bridged
//        if (!user->bridged().isEmpty())
//            tooltip += "<br>" + tr("Bridged from: %1").arg(user->bridged());
        return tooltip;
    }

    if (role == Qt::ForegroundRole) {
        // FIXME: boilerplate with TimelineItem.qml:57
        const auto& palette = view->palette();
        return QColor::fromHslF(static_cast<float>(m.hueF()),
                                1 - palette.color(QPalette::Window).saturationF(),
                                0.9f - 0.7f * palette.color(QPalette::Window).lightnessF(),
                                palette.color(QPalette::ButtonText).alphaF());
    }

    return QVariant();
}

int UserListModel::rowCount(const QModelIndex& parent) const
{
    if( parent.isValid() )
        return 0;

    return m_memberIds.count();
}

void UserListModel::userAdded(const RoomMember& member)
{
    auto pos = findUserPos(member.id());
    if (pos != m_memberIds.size() && m_memberIds[pos] == member.id())
    {
        qCWarning(MODELS) << "Trying to add the user" << member.id()
                          << "but it's already in the user list";
        return;
    }
    beginInsertRows(QModelIndex(), pos, pos);
    m_memberIds.insert(pos, member.id());
    endInsertRows();
}

void UserListModel::userRemoved(const RoomMember& member)
{
    auto pos = findUserPos(member);
    if (pos == m_memberIds.size())
    {
        qCWarning(MODELS)
            << "Trying to remove a room member not in the user list:"
            << member.id();
        return;
    }

    beginRemoveRows(QModelIndex(), pos, pos);
    m_memberIds.removeAt(pos);
    endRemoveRows();
}

void UserListModel::filter(const QString& filterString)
{
    if (m_currentRoom == nullptr)
        return;

    QElapsedTimer et; et.start();

    beginResetModel();
    auto filteredMembers = Quotient::rangeTo<QList>(
        std::views::filter(m_currentRoom->joinedMembers(),
                           Quotient::memberMatcher(filterString, Qt::CaseInsensitive)));
    std::ranges::sort(filteredMembers, Quotient::MemberSorter());
    const auto sortedIds = std::views::transform(filteredMembers, &RoomMember::id);
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    m_memberIds.assign(sortedIds.begin(), sortedIds.end());
#else
    m_memberIds = QList(sortedIds.begin(), sortedIds.end());
#endif
    endResetModel();

    qCDebug(MODELS) << "Filtering" << m_memberIds.size() << "user(s) in"
                    << m_currentRoom->displayName() << "took" << et;
}

void UserListModel::refresh(const RoomMember& member, QVector<int> roles)
{
    auto pos = findUserPos(member);
    if ( pos != m_memberIds.size() )
        emit dataChanged(index(pos), index(pos), roles);
    else
        qCWarning(MODELS)
            << "Trying to access a room member not in the user list";
}

void UserListModel::avatarChanged(const RoomMember& m)
{
    refresh(m, {Qt::DecorationRole});
}

int UserListModel::findUserPos(const Quotient::RoomMember& m) const
{
    return findUserPos(m.disambiguatedName());
}

int UserListModel::findUserPos(const QString& username) const
{
    return static_cast<int>(Quotient::lowerBoundMemberIndex(m_memberIds, username, m_currentRoom));
}
