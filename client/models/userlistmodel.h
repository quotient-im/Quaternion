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
    class User;
}

class UserListModel: public QAbstractListModel
{
        Q_OBJECT
    public:
        using User = Quotient::User;

        UserListModel(QAbstractItemView* parent);
        virtual ~UserListModel();

        void setRoom(Quotient::Room* room);
        User* userAt(QModelIndex index);

        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        int rowCount(const QModelIndex& parent=QModelIndex()) const override;
    
    signals:
        void membersChanged(); //< Reflection of Room::memberListChanged

    public slots:
        void filter(const QString& filterString);

    private slots:
        void userAdded(User* user);
        void userRemoved(User* user);
        void refresh(User* user, QVector<int> roles = {});
        void avatarChanged(User* user);

    private:
        Quotient::Room* m_currentRoom;
        QList<User*> m_users;

        int findUserPos(User* user) const;
        int findUserPos(const QString& username) const;
};
