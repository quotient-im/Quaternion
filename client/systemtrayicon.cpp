/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2016 Felix Rohrbach <kde@fxrh.de>              *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#include "systemtrayicon.h"

#include <QtGui/QWindow>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>

#include "mainwindow.h"
#include "quaternionroom.h"
#include "desktop_integration.h"

#include <Quotient/settings.h>
#include <Quotient/qt_connection_util.h>

using namespace Qt::StringLiterals;

SystemTrayIcon::SystemTrayIcon(MainWindow* parent)
    : QSystemTrayIcon(parent)
    , m_parent(parent)
{
    auto contextMenu = new QMenu(parent);
    auto showHideAction =
        contextMenu->addAction(tr("Hide"), this, &SystemTrayIcon::showHide);
    contextMenu->addAction(tr("Quit"), this, QApplication::quit);
    m_parent->winId(); // To make sure m_parent->windowHandle() is initialised
    connect(m_parent->windowHandle(), &QWindow::visibleChanged, [showHideAction](bool visible) {
        showHideAction->setText(visible ? tr("Hide") : tr("Show"));
    });

    setIcon(appIcon());
    setToolTip("Quaternion");
    setContextMenu(contextMenu);
    connect( this, &SystemTrayIcon::activated, this, &SystemTrayIcon::systemTrayIconAction);
}

void SystemTrayIcon::newRoom(Quotient::Room* room)
{
    unreadStatsChanged(room);
    connect(room, &Quotient::Room::unreadStatsChanged,
            this, [this,room] { unreadStatsChanged(room); });
}

void SystemTrayIcon::unreadStatsChanged(Quotient::Room* room)
{
    using namespace Quotient;
    const auto mode = Settings().get<QString>("UI/notifications", "intrusive");
    int nNotifs = 0;

    if (qApp->activeWindow() != nullptr || room->notificationCount() == 0)
        return;

    for (auto* c: m_parent->registry()->accounts())
        for (auto* r: c->allRooms())
            nNotifs += r->notificationCount();
    setToolTip(tr("%Ln notification(s)", "", nNotifs));

    if (!m_notified) {
        static const auto unreadIcon = QIcon::fromTheme(u"mail-unread"_s, appIcon());
        setIcon(unreadIcon);
        m_notified = true;
        if (mode == "none")
            return;

        showMessage(
            //: %1 is the room display name
            tr("Notification in %1").arg(room->displayName()),
            tr("%Ln notification(s)", "", room->notificationCount()));
        if (mode != "non-intrusive")
            m_parent->activateWindow();
        connectSingleShot(this, &SystemTrayIcon::messageClicked, m_parent,
                          [this,qRoom=static_cast<QuaternionRoom*>(room)]
                          { m_parent->selectRoom(qRoom); });
    }
}

void SystemTrayIcon::systemTrayIconAction(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger
        || reason == QSystemTrayIcon::DoubleClick)
        showHide();
}

void SystemTrayIcon::showHide()
{
    if (m_parent->isVisible())
        m_parent->hide();
    else {
        m_parent->show();
        m_parent->activateWindow();
        m_parent->raise();
        m_parent->setFocus();
    }
}

void SystemTrayIcon::focusChanged(QWidget* old)
{
    if (m_notified && old == nullptr && qApp->activeWindow() != nullptr) {
        setIcon(appIcon());
        setToolTip("Quaternion");
        m_notified = false;
    }
}
