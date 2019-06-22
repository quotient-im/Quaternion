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

#include "popupnotifier.h"

#include "mainwindow.h"
#include "systemtrayicon.h"
#include "models/roomlistmodel.h"
#include <user.h>
#include <settings.h>

#include <QDebug>

using namespace Quotient;

PopupNotifier::PopupNotifier(MainWindow* parent, RoomListModel* roomlistmodel, SystemTrayIcon* systemTrayIcon)
    : QObject(parent)
    , m_parent(parent)
    , m_roomlistmodel(roomlistmodel)
    , m_systemtrayicon(systemTrayIcon)
{
    connect(roomlistmodel, &RoomListModel::rowsInserted, this,
        [this](QModelIndex parent, int first, int last) {
            qDebug() << "PopupNotifier: RoomListModel: rowsInserted";
            for (int i = first; i <= last; i++) {
                auto idx = m_roomlistmodel->index(i, 0, parent);
                auto room = qvariant_cast<QuaternionRoom*>(m_roomlistmodel->data(idx, RoomListModel::ObjectRole));
                if (room)
                    connectRoomSignals(room);
            }
        }
    );
}

void PopupNotifier::connectRoomSignals(QuaternionRoom* room)
{
    qDebug() << "PopupNotifier::connectRoomSignals:" << room;

    connect(room, &Room::highlightCountChanged,
            this, [this,room] { highlightCountChanged(room); });
    connect(room, &Room::notificationCountChanged,
            this, [this,room] { notificationCountChanged(room); });
    connect(room, &Room::addedMessages,
            this, [this,room](int from, int to) { addedMessages(room, from, to); });
}

void PopupNotifier::showMessage(Room* room, const QString &title, const QString &message)
{
    // we could use libnotify here on Linux
    if (m_systemtrayicon && m_systemtrayicon->supportsMessages()) {
        m_systemtrayicon->showMessage(title, message);
        connectSingleShot(m_systemtrayicon, &SystemTrayIcon::messageClicked, m_parent,
                          [this,room] { m_parent->selectRoom(room); });
    }
}

void PopupNotifier::highlightCountChanged(QuaternionRoom* room)
{
    if (popupMode() == "server" && room->highlightCount() > 0) {
        showMessage(room,
                    tr("Highlight in %1").arg(room->displayName()),
                    tr("%n highlight(s) in total", "", room->highlightCount()));

        if (hasPopupTweak(QStringList{"intrusive"}))
            m_parent->activateWindow();
    }
}

void PopupNotifier::notificationCountChanged(QuaternionRoom* room)
{
    if (popupMode() == "server" && room->notificationCount() > 0 &&
            !hasPopupTweak(QStringList{"highlight+"}))
    {
        showMessage(room,
                    tr("Notification in %1").arg(room->displayName()),
                    tr("%n notification(s) in total", "", room->notificationCount()));
    }
}

void PopupNotifier::addedMessages(QuaternionRoom* room, int lowest, int biggest)
{
    if (popupMode() != "client")
        return;

    qDebug() << "PopupNotifier: addedMessages:" << room;
    qDebug() << "timelineSize:" << room->timelineSize()
            << "minTimelineIndex:" << room->minTimelineIndex()
            << "maxTimelineIndex:" << room->maxTimelineIndex()
            << "lowest:" << lowest
            << "biggest:" << biggest;

    bool backfill = lowest < 0 || biggest < 0;
    if (backfill)
        return;

    Q_ASSERT(lowest  >= room->minTimelineIndex());
    Q_ASSERT(biggest <= room->maxTimelineIndex());

    auto it = room->messageEvents().end()-1 - room->maxTimelineIndex() + lowest;
    auto itEnd = room->messageEvents().end()-1 - room->maxTimelineIndex() + biggest;
    do {
        const RoomEvent& evt = **it;
        qDebug() << "timestamp:" << evt.timestamp();

        if (const RoomMessageEvent* e = eventCast<const RoomMessageEvent>(&evt)) {
            auto senderName = room->user(evt.senderId())->displayname(room);
            auto roomName = room->displayName();
            bool isHighlight = room->isEventHighlighted(&evt) || room->isDirectChat();
            if (
                    (!room->isLowPriority() || hasPopupTweak(QStringList{"lowprio"})) &&
                    (isHighlight || !hasPopupTweak(QStringList{"highlights+"})) &&
                    !m_parent->isActiveWindow()
            ) {
                if (e->msgtype() == MessageEventType::Image) {
                    qDebug() << "image";
                } else if (e->hasFileContent()) {
                    qDebug() << "file";
                } else {
                    QString msg, title;

                    title = room->isDirectChat() ?
                            tr("Private message from %1").arg(senderName) :
                            tr("Message in %1").arg(roomName);

                    if (e->msgtype() == MessageEventType::Notice)
                        msg = e->plainBody() + " (notice)";
                    else if (e->msgtype() == MessageEventType::Emote)
                        msg = "/me " + e->plainBody();
                    else
                        msg = e->plainBody();

                    if (!room->isDirectChat())
                        msg.prepend(senderName + "> ");

                    showMessage(room, title, msg);
                }

                if (hasPopupTweak(QStringList{"intrusive"}) && isHighlight)
                    m_parent->activateWindow();
            }
        }
        if (evt.isStateEvent())
            qDebug() << "stateEvent";
    } while (it++ != itEnd);
}

const QString PopupNotifier::popupMode()
{
    return qvariant_cast<QString>(Quotient::SettingsGroup("Notification").value("popup_mode", "client"));
}

bool PopupNotifier::hasPopupTweak(const QStringList &flags)
{
    auto popup_tweaks = SettingsGroup("Notification").get<QStringList>("popup_tweaks");
    qDebug() << "popup_tweaks:" << popup_tweaks;

    for (const auto flag : flags) {
        qDebug() << "checking for" << flag;
        if (popup_tweaks.contains(flag))
            return true;
    }
    return false;
}
