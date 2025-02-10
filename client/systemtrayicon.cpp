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
#include <QtCore/QFuture>

#include "mainwindow.h"
#include "quaternionroom.h"
#include "desktop_integration.h"

#include <Quotient/settings.h>
#include <Quotient/qt_connection_util.h>

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
    connect(room, &Quotient::Room::highlightCountChanged,
            this, [this,room] { highlightCountChanged(room); });
}

void SystemTrayIcon::highlightCountChanged(Quotient::Room* room)
{
    using namespace Quotient;
    const auto mode = Settings().get<QString>("UI/notifications", "intrusive");
    if (mode == "none")
        return;
    if( room->highlightCount() > 0 ) {
        showMessage(
            //: %1 is the room display name
            tr("Highlight in %1").arg(room->displayName()),
            tr("%Ln highlight(s)", "", room->highlightCount()));
        if (mode != "non-intrusive")
            m_parent->activateWindow();
        QtFuture::connect(this, &SystemTrayIcon::messageClicked)
            .then(m_parent, std::bind_front(&MainWindow::selectRoom, m_parent,
                                            static_cast<QuaternionRoom*>(room)));
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
