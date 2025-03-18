/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2015 Felix Rohrbach <kde@fxrh.de>                        *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#pragma once

#include <QtCore/QAbstractListModel>

class QAbstractItemView;

namespace Quotient
{
    class Connection;
    class Room;
    class RoomMember;
}

class UserListModel: public QAbstractListModel
{
        Q_OBJECT
    public:
        UserListModel(QAbstractItemView* parent);

        void setRoom(Quotient::Room* room);
        Quotient::RoomMember userAt(QModelIndex index) const;

        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        int rowCount(const QModelIndex& parent=QModelIndex()) const override;

    signals:
        void membersChanged(); //!< Reflection of Room::memberListChanged

    public slots:
        void filter(const QString& filterString);

    private slots:
        void userAdded(const Quotient::RoomMember& member);
        void userRemoved(const Quotient::RoomMember& member);
        void refresh(const Quotient::RoomMember& member, QVector<int> roles = {});
        void avatarChanged(const Quotient::RoomMember& m);

    private:
        Quotient::Room* m_currentRoom;
        QList<QString> m_memberIds;

        int findUserPos(const Quotient::RoomMember &m) const;
        int findUserPos(const QString& username) const;
        void doFilter(const QString& filterString);
};
