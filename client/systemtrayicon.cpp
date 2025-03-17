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

SystemTrayIcon::SystemTrayIcon(MainWindow* parent) : QSystemTrayIcon(parent)
{
    auto contextMenu = new QMenu(parent);
    auto showHideAction =
        contextMenu->addAction(tr("Hide"), this, &SystemTrayIcon::showHide);
    contextMenu->addAction(tr("Quit"), this, QApplication::quit);
    mainWindow()->winId(); // To make sure mainWindow()->windowHandle() is initialised
    connect(mainWindow()->windowHandle(), &QWindow::visibleChanged, [showHideAction](bool visible) {
        showHideAction->setText(visible ? tr("Hide") : tr("Show"));
    });

    setIcon(appIcon());
    setToolTip("Quaternion");
    setContextMenu(contextMenu);
    connect(this, &SystemTrayIcon::activated, this, &SystemTrayIcon::systemTrayIconAction);
    connect(qApp, &QApplication::focusChanged, this, &SystemTrayIcon::focusChanged);
}

void SystemTrayIcon::newRoom(Quotient::Room* room)
{
    unreadStatsChanged();
    highlightCountChanged(room);
    connect(room, &Quotient::Room::unreadStatsChanged, this, &SystemTrayIcon::unreadStatsChanged);
    connect(room, &Quotient::Room::highlightCountChanged, this,
            [this, room] { highlightCountChanged(room); });
}

void SystemTrayIcon::unreadStatsChanged()
{
    const auto mode = notificationMode();
    if (mode == u"none")
        return;

    int nNotifs = 0;
    for (auto* c: mainWindow()->registry()->accounts())
        for (auto* r: c->allRooms())
            nNotifs += r->notificationCount();
    setToolTip(tr("%Ln unread message(s) across all rooms", "", nNotifs));

    if (m_notified || qApp->activeWindow() != nullptr)
        return;

    if (nNotifs == 0) {
        setIcon(appIcon());
        return;
    }

    static const auto unreadIcon = QIcon::fromTheme(u"mail-unread"_s, appIcon());
    setIcon(unreadIcon);
    m_notified = true;
}

void SystemTrayIcon::highlightCountChanged(Quotient::Room* room)
{
    if (qApp->activeWindow() != nullptr || room->highlightCount() == 0)
        return;

    const auto mode = notificationMode();
    if (mode == u"none")
        return;

    //: %1 is the room display name
    showMessage(tr("Highlight in %1").arg(room->displayName()),
                tr("%Ln highlight(s)", "", static_cast<int>(room->highlightCount())));
    if (mode == u"intrusive")
        mainWindow()->activateWindow();

    connect(this, &SystemTrayIcon::messageClicked, mainWindow(),
            [this, r = static_cast<QuaternionRoom*>(room)] { mainWindow()->selectRoom(r); },
            Qt::SingleShotConnection);
}

void SystemTrayIcon::systemTrayIconAction(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger
        || reason == QSystemTrayIcon::DoubleClick)
        showHide();
}

void SystemTrayIcon::showHide()
{
    if (mainWindow()->isVisible())
        mainWindow()->hide();
    else {
        mainWindow()->show();
        mainWindow()->activateWindow();
        mainWindow()->raise();
        mainWindow()->setFocus();
    }
}

MainWindow* SystemTrayIcon::mainWindow() const { return static_cast<MainWindow*>(parent()); }

QString SystemTrayIcon::notificationMode() const
{
    static const Quotient::Settings settings{};
    return settings.get<QString>("UI/notifications", u"intrusive"_s);
}

void SystemTrayIcon::focusChanged(QWidget* old)
{
    if (m_notified && old == nullptr && qApp->activeWindow() != nullptr) {
        setIcon(appIcon());
        m_notified = false;
    }
}
