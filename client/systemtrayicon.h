/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2016 Felix Rohrbach <kde@fxrh.de>              *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#pragma once

#include <QtWidgets/QSystemTrayIcon>

namespace Quotient
{
    class Room;
}

class MainWindow;

class SystemTrayIcon: public QSystemTrayIcon
{
        Q_OBJECT
    public:
        explicit SystemTrayIcon(MainWindow* parent = nullptr);

    public slots:
        void newRoom(Quotient::Room* room);

    private slots:
        void unreadStatsChanged(Quotient::Room* room);
        void systemTrayIconAction(QSystemTrayIcon::ActivationReason reason);
        void focusChanged(QWidget* old);

    private:
        MainWindow* m_parent;
        QIcon m_appIcon;
        QIcon m_unreadIcon;
        bool m_notified;
        void showHide();
};
