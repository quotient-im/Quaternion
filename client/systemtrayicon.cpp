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

#include "systemtrayicon.h"

#include "mainwindow.h"
#include "models/roomlistmodel.h"
#include "quaternionroom.h"
#include <settings.h>
#include <qt_connection_util.h>
#include <functional>
#include <QPainter>

// Based on Spectral's src/trayicon.cpp
ComposedTrayIcon::ComposedTrayIcon(const QString& filename) : QIconEngine()
{
    icon_ = QIcon(filename);
}

void ComposedTrayIcon::paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state)
{
    painter->setRenderHint(QPainter::Antialiasing);
    icon_.paint(painter, rect, Qt::AlignCenter, mode, state);

    if (hasInvite || hasHighlight || hasNotification) {
        QBrush brush;
        brush.setStyle(Qt::SolidPattern);
        brush.setColor(QColor(hasInvite ? "#0acb00" : hasHighlight ? "red" : "#df52df"));
        painter->setBrush(brush);
        QRectF bubble(rect.width() - BubbleDiameter, 0, BubbleDiameter, BubbleDiameter);
        painter->drawEllipse(bubble);
    }
}

QIconEngine* ComposedTrayIcon::clone() const
{
    return new ComposedTrayIcon(*this);
}

QList<QSize> ComposedTrayIcon::availableSizes(QIcon::Mode mode, QIcon::State state) const
{
    Q_UNUSED(mode)
    Q_UNUSED(state)
    QList<QSize> sizes;
    sizes.append(QSize(24, 24));
    sizes.append(QSize(32, 32));
    sizes.append(QSize(48, 48));
    sizes.append(QSize(64, 64));
    sizes.append(QSize(128, 128));
    sizes.append(QSize(256, 256));
    return sizes;
}

QPixmap ComposedTrayIcon::pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state)
{
    QImage img(size, QImage::Format_ARGB32);
    img.fill(qRgba(0, 0, 0, 0));
    QPixmap result = QPixmap::fromImage(img, Qt::NoFormatConversion);
    {
        QPainter painter(&result);
        paint(&painter, QRect(QPoint(0, 0), size), mode, state);
    }
    return result;
}


SystemTrayIcon::SystemTrayIcon(MainWindow* parent, RoomListModel* roomlistmodel)
    : QSystemTrayIcon(parent)
    , m_parent(parent)
    , m_roomlistmodel(roomlistmodel)
{
    m_icon = new ComposedTrayIcon(":/icon.png");
    setIcon(QIcon(m_icon));
    setToolTip("Quaternion");
    connect( this, &SystemTrayIcon::activated, this, &SystemTrayIcon::systemTrayIconAction);

    connect(roomlistmodel, &RoomListModel::dataChanged, this,
            [this](QModelIndex idx1, QModelIndex idx2, const QVector<int>& roles) {
                //qDebug() << "dataChanged@" << idx1 << idx2 << roles;
                evaluate();
            }
    );
    connect(roomlistmodel, &RoomListModel::rowsInserted, this,
            [this](QModelIndex parent, int first, int last) {
                //qDebug() << "rowsInserted";
                evaluate();
            }
    );
    connect(roomlistmodel, &RoomListModel::rowsRemoved, this,
            [this](QModelIndex parent, int first, int last) {
                //qDebug() << "rowsRemoved";
                evaluate();
            }
    );

    show();
}

template<typename T>
bool roomIndicesForeachCallback(std::function<T(const QModelIndex *)> callback, const QModelIndex *message)
{
    callback(message);
    return false;
}
template<>
bool roomIndicesForeachCallback<bool>(std::function<bool(const QModelIndex *)> callback, const QModelIndex *index)
{
    return callback(index);
}

template<typename T>
void SystemTrayIcon::roomIndicesForeach(std::function<T(const QModelIndex *)> callback) const
{
    Q_ASSERT(m_roomlistmodel->columnCount(QModelIndex()) == 1);

    for (int i = 0; i < m_roomlistmodel->rowCount(QModelIndex()); i++) {
        auto groupindex = m_roomlistmodel->index(i, 0);
        for (int j = 0; j < m_roomlistmodel->rowCount(groupindex); j++) {
            auto index = m_roomlistmodel->index(j, 0, groupindex);
            if (roomIndicesForeachCallback(callback, &index))
                return;
        }
    }
}

void SystemTrayIcon::evaluate()
{
    bool hasInvite = false;
    bool hasHighlight = false;
    bool hasNotification = false;
    bool hasUnread = false;
    roomIndicesForeach<void>([&hasInvite,&hasHighlight,&hasNotification,&hasUnread](const QModelIndex *index) {
        hasInvite |= index->data(RoomListModel::JoinStateRole).toString() == "invite";
        hasHighlight |= index->data(RoomListModel::HighlightCountRole).toInt() > 0;
        hasNotification |= index->data(RoomListModel::NotificationCountRole).toInt() > 0;
        hasUnread |= index->data(RoomListModel::HasUnreadRole).toBool();
    });
    qDebug() << "evaluate; hasInvite:" << hasInvite << "hasHighlight:" << hasHighlight << "hasNotification" << hasNotification;

    ComposedTrayIcon* icon = static_cast<ComposedTrayIcon*>(m_icon->clone());
    icon->hasInvite = hasInvite;
    icon->hasHighlight = hasHighlight;
    icon->hasNotification = hasNotification;
    icon->hasUnread = hasUnread;
    m_icon = icon;
    setIcon(QIcon(m_icon));

    if (hasInvite || hasHighlight || hasNotification) {
        auto hasTypes = QStringList();
        if (hasInvite)
            hasTypes << tr("undecided invitation");
        if (hasHighlight)
            hasTypes << tr("unseen highlights");
        if (hasNotification)
            hasTypes << tr("unseen notifications");
        setToolTip(tr("Quaternion: you have ") + hasTypes.join(QStringLiteral(", ")));
    } else {
        setToolTip(tr("Quaternion"));
    }
}

void SystemTrayIcon::systemTrayIconAction(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason)
    {
        case QSystemTrayIcon::Trigger:
        case QSystemTrayIcon::DoubleClick:
            this->showHide();
            break;
        default:
            ;
    }
}

void SystemTrayIcon::selectRoomByIndex(const QModelIndex *index)
{
    auto room = qvariant_cast<QuaternionRoom*>(index->data(RoomListModel::ObjectRole));
    m_parent->selectRoom(room);
}

void SystemTrayIcon::selectNextRoom()
{
    bool accepted = false;

    if (!accepted)
        roomIndicesForeach<bool>([this,&accepted](const QModelIndex *index) {
            if (index->data(RoomListModel::JoinStateRole).toString() == "invite") {
                selectRoomByIndex(index);
                return accepted = true;
            }
            return false;
        });

    if (!accepted)
        roomIndicesForeach<bool>([this,&accepted](const QModelIndex *index) {
            if (index->data(RoomListModel::HighlightCountRole).toInt() > 0) {
                selectRoomByIndex(index);
                return accepted = true;
            }
            return false;
        });

    if (!accepted)
        roomIndicesForeach<bool>([this,&accepted](const QModelIndex *index) {
            if (index->data(RoomListModel::NotificationCountRole).toInt() > 0) {
                selectRoomByIndex(index);
                return accepted = true;
            }
            return false;
        });
}

void SystemTrayIcon::showHide()
{
    if (m_parent->isVisible() && !(m_parent->windowState() & Qt::WindowMinimized) &&
        m_parent->isActiveWindow() &&
        !m_icon->hasInvite && !m_icon->hasHighlight && !m_icon->hasNotification)
    {
        m_parent->hide();
    }
    else
    {
        m_parent->setWindowState(m_parent->windowState() & ~Qt::WindowMinimized);
        m_parent->show();
        m_parent->activateWindow();
        m_parent->raise();
        selectNextRoom();
    }
}
