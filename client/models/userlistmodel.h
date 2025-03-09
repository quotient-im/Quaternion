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
        using RoomMember = Quotient::RoomMember;

        UserListModel(QAbstractItemView* parent);

        void setRoom(Quotient::Room* room);
        RoomMember userAt(QModelIndex index) const;

        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        int rowCount(const QModelIndex& parent=QModelIndex()) const override;

    signals:
        void membersChanged(); //!< Reflection of Room::memberListChanged

    public slots:
        void filter(const QString& filterString);

    private slots:
        void userAdded(const RoomMember& member);
        void userRemoved(const RoomMember& member);
        void refresh(const RoomMember& member, QVector<int> roles = {});
        void avatarChanged(const RoomMember& m);

    private:
        Quotient::Room* m_currentRoom;
        QList<QString> m_memberIds;

        int findUserPos(const RoomMember &m) const;
        int findUserPos(const QString& username) const;
};
