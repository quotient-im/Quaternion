/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2015 Felix Rohrbach <kde@fxrh.de>                        *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#include "userlistmodel.h"

#include "../logging_categories.h"
#include "../quaternionroom.h"

#include <QtCore/QDebug>
#include <QtGui/QFontMetrics>
#include <QtGui/QPalette>
#include <QtGui/QPixmap>
// Injecting the dependency on a view is not so nice; but the way the model provides avatar
// decorations depends on the delegate size, and some other defaults come from the view, too
#include <QtWidgets/QAbstractItemView>

#include <Quotient/connection.h>
#include <Quotient/ranges_extras.h>
#include <Quotient/room.h>
#include <Quotient/user.h>

using Quotient::RoomMember;

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
        doFilter({});
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

    if (QUO_ALARM(index.row() >= m_memberIds.count()))
        return QVariant();

    auto m = userAt(index);
    const auto* view = static_cast<const QAbstractItemView*>(parent());

    switch (role) {
    case Qt::DisplayRole:    return m.displayName();
    case Qt::DecorationRole: {
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
    case Qt::ToolTipRole: {
        auto tooltip =
          QLatin1String("<b>%1</b><br>%2<br>").arg(m.name().toHtmlEscaped(), m.id().toHtmlEscaped())
          + tr("Power level: %1 (%2)")
              .arg(m.powerLevel())
              .arg(static_cast<QuaternionRoom*>(m_currentRoom)->powerGrade(m));
        // TODO: Find a new way to determine that the user is bridged
//        if (!user->bridged().isEmpty())
//            tooltip += "<br>" + tr("Bridged from: %1").arg(user->bridged());
        return tooltip;
    }
    case Qt::ForegroundRole: {
        // FIXME: boilerplate with TimelineItem.qml:57
        const auto& palette = view->palette();
        return QColor::fromHslF(static_cast<float>(m.hueF()),
                                1 - palette.color(QPalette::Window).saturationF(),
                                0.9f - 0.7f * palette.color(QPalette::Window).lightnessF(),
                                palette.color(QPalette::ButtonText).alphaF());
    }
    case Qt::FontRole: {
        auto font = view->font();
        if (m_currentRoom->creatorIds().contains(m.id())) {
            font.setBold(true);
        }
        return font;
    }
    default: return QVariant();
    }
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

    beginResetModel();
    doFilter(filterString);
    endResetModel();
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

struct MemberSorter {
    bool operator()(const RoomMember& m1, const RoomMember& m2) const
    {
#if Quotient_VERSION_MINOR < 10
        if (QUO_ALARM(!room))
            return false;
        const auto m1IsCreator = room->creatorIds().contains(m1.id());
        const auto m2IsCreator = room->creatorIds().contains(m2.id());
#else
        const auto m1IsCreator = m1.isCreator();
        const auto m2IsCreator = m2.isCreator();
#endif
        if (m1IsCreator != m2IsCreator)
            return m1IsCreator > m2IsCreator;

        return ms(m1, m2);
    }

#if Quotient_VERSION_MINOR < 10
    const Quotient::Room* room;
#endif
    Quotient::MemberSorter ms{};
};

int UserListModel::findUserPos(const Quotient::RoomMember& m) const
{
    return static_cast<int>(
      std::ranges::lower_bound(m_memberIds, m,
#if Quotient_VERSION_MINOR < 10
                               MemberSorter{m_currentRoom},
#else
                               MemberSorter{},
#endif
                               std::bind_front(&Quotient::Room::member, m_currentRoom))
      - m_memberIds.begin());
}

int UserListModel::findUserPos(const Quotient::UserId& mxId) const
{
    return findUserPos(m_currentRoom->member(mxId));
}

void UserListModel::doFilter(const QString& filterString)
{
    QElapsedTimer et; et.start();

    auto filteredMembers = Quotient::rangeTo<QList>(
        std::views::filter(m_currentRoom->joinedMembers(),
                           Quotient::memberMatcher(filterString, Qt::CaseInsensitive)));
#if Quotient_VERSION_MINOR < 10
    std::ranges::sort(filteredMembers, MemberSorter{m_currentRoom});
#else
    std::ranges::sort(filteredMembers, MemberSorter{});
#endif
    const auto sortedIds = std::views::transform(filteredMembers, &RoomMember::id);
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    m_memberIds.assign(sortedIds.begin(), sortedIds.end());
#else
    m_memberIds = QList(sortedIds.begin(), sortedIds.end());
#endif

    qCDebug(MODELS) << "Filtering" << m_memberIds.size() << "user(s) in"
                    << m_currentRoom->displayName() << "took" << et;
}
