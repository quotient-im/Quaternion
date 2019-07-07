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

#pragma once

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QDockWidget>

namespace Quotient
{
    class User;
}

class UserListModel;
class QuaternionRoom;
class QTableView;
class QMenu;
class QLineEdit;

class UserListDock: public QDockWidget
{
        Q_OBJECT
    public:
        explicit UserListDock(QWidget* parent = nullptr);

        void setRoom( QuaternionRoom* room );

    signals:
        void userMentionRequested(Quotient::User* u);

    private slots:
        void refreshTitle();
        void showContextMenu(QPoint pos);
        void startChatSelected();
        void requestUserMention();
        void kickUser();
        void banUser();
        void ignoreUser();
        bool isIgnored();

    private:
        QWidget* m_widget;
        QVBoxLayout* m_box;
        QTableView* m_view;
        QLineEdit* m_filterline;
        UserListModel* m_model;
        QuaternionRoom* m_currentRoom = nullptr;

        QMenu* contextMenu;
        QAction* ignoreAction;

        Quotient::User* getSelectedUser() const;
};
