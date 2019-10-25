/**************************************************************************
 *                                                                        *
 * Copyright (C) 2019 Roland Pallai <dap78@magex.hu>                      *
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

#ifndef POPUPNOTIFIER_H
#define POPUPNOTIFIER_H

#include "systemtrayicon.h"
#include "quaternionroom.h"

class PopupNotifier : public QObject {
    Q_OBJECT

public:
    PopupNotifier(MainWindow* parent, RoomListModel* roomlistmodel, SystemTrayIcon* systemTrayIcon);

private slots:
    void highlightCountChanged(QuaternionRoom* room);
    void notificationCountChanged(QuaternionRoom* room);
    void addedMessages(QuaternionRoom* room, int fromIndex, int toIndex);

private:
    MainWindow* m_parent;
    RoomListModel* m_roomlistmodel;
    SystemTrayIcon* m_systemtrayicon;

    void connectRoomSignals(QuaternionRoom* room);
    void showMessage(Quotient::Room* room, const QString &title, const QString &message);
    const QString popupMode();
    bool hasPopupTweak(const QStringList &flags);
};

#endif /* POPUPNOTIFIER_H */
