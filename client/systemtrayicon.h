/**************************************************************************
 *                                                                        *
 * Copyright (C) 2016 Felix Rohrbach <kde@fxrh.de>                        *
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
        void highlightCountChanged(Quotient::Room* room);
        void systemTrayIconAction(QSystemTrayIcon::ActivationReason reason);

    private:
        MainWindow* m_parent;
        void showHide();
};
