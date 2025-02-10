/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2015 Felix Rohrbach <kde@fxrh.de>              *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#pragma once

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QDockWidget>

class UserListModel;
class QuaternionRoom;
class QTableView;
class QLineEdit;

class UserListDock: public QDockWidget
{
        Q_OBJECT
    public:
        explicit UserListDock(QWidget* parent = nullptr);

        void setRoom( QuaternionRoom* room );

    signals:
        void userMentionRequested(QString userId);

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

        QString getSelectedUser() const;
};
