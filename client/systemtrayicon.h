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
#include <QIconEngine>
#include <functional>

namespace Quotient
{
    class Room;
}

class MainWindow;
class RoomListModel;

class ComposedTrayIcon : public QIconEngine {
public:
    ComposedTrayIcon(const QString& filename);

    virtual void paint(QPainter* p, const QRect& rect, QIcon::Mode mode, QIcon::State state);
    virtual QIconEngine* clone() const;
    virtual QList<QSize> availableSizes(QIcon::Mode mode, QIcon::State state) const;
    virtual QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state);

    bool hasInvite;
    bool hasNotification;
    bool hasHighlight;
    bool hasUnread;

private:
    const int BubbleDiameter = 8;

    QIcon icon_;
};

class SystemTrayIcon: public QSystemTrayIcon
{
        Q_OBJECT
    public:
        explicit SystemTrayIcon(MainWindow* parent, RoomListModel* roomlistmodel);

    public slots:
        void evaluate();

    private slots:
        void systemTrayIconAction(QSystemTrayIcon::ActivationReason reason);
        void selectNextRoom();

    private:
        MainWindow* m_parent;
        RoomListModel* m_roomlistmodel;
        ComposedTrayIcon* m_icon;
        void showHide();
        template<typename T> void roomIndicesForeach(std::function<T(const QModelIndex *)> callback) const;
        void selectRoomByIndex(const QModelIndex *index);
};
