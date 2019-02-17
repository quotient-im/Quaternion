/**************************************************************************
 *                                                                        *
 * Copyright (C) 2016 Malte Brandy <malte.brandy@maralorn.de>             *
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

#include "activitydetector.h"

#include <QtCore/QDebug>

#include "mainwindow.h"
#include "chatroomwidget.h"

ActivityDetector::ActivityDetector(QApplication& a, MainWindow& w)
    : m_app(a)
    , m_mainWindow(w)
    , m_enabled(false)
{
    const auto chatWidget = w.getChatRoomWidget();
    connect( chatWidget, &ChatRoomWidget::readMarkerMoved,
             this, &ActivityDetector::updateEnabled );
    connect( chatWidget, &ChatRoomWidget::readMarkerCandidateMoved,
             this, &ActivityDetector::updateEnabled );
    connect( this, &ActivityDetector::triggered,
             chatWidget, &ChatRoomWidget::markShownAsRead );
}

void ActivityDetector::updateEnabled()
{
    setEnabled(m_mainWindow.getChatRoomWidget()->pendingMarkRead());
}

void ActivityDetector::setEnabled(bool enabled)
{
    if (enabled == m_enabled)
        return;

    m_enabled = enabled;
    m_mainWindow.setMouseTracking(enabled);
    if (enabled)
        m_app.installEventFilter(this);
    else
        m_app.removeEventFilter(this);
    qDebug() << "Activity Detector enabled:" << enabled;
}

bool ActivityDetector::eventFilter(QObject* obj, QEvent* ev)
{
    switch (ev->type())
    {
    case QEvent::KeyPress:
    case QEvent::FocusIn:
    case QEvent::MouseMove:
    case QEvent::MouseButtonPress:
        emit triggered();
        break;
    default:;
    }
    return QObject::eventFilter(obj, ev);
}
