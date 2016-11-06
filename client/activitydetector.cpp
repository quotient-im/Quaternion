/**************************************************************************
 *                                                                        *
 * Copyright (C) 2016 Malte Brandy <malte.brandy@maralorn.de>                        *
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
#include "mainwindow.h"
#include "chatroomwidget.h"
#include "models/messageeventmodel.h"
#include <QtCore/QDebug>

ActivityDetector::ActivityDetector(QApplication* a, MainWindow* w): m_app(a), m_mainWindow(w), m_enabled(false), m_messageEventModel(w->getChatRoomWidget()->getMessageEventModel())
{
    connect(this, &ActivityDetector::triggered, m_messageEventModel, &MessageEventModel::markShownAsRead);
    connect(m_messageEventModel, &MessageEventModel::lastShownIndexChanged, this, &ActivityDetector::updateEnabled);
    connect(m_messageEventModel, &MessageEventModel::readMarkerIndexChanged, this, &ActivityDetector::updateEnabled);
}

void ActivityDetector::updateEnabled()
{
    setEnabled(m_messageEventModel->awaitingMarkRead());
}

void ActivityDetector::setEnabled(bool enabled)
{
    if (enabled && !m_enabled) {
        m_app->installEventFilter(this);
        m_mainWindow->setMouseTracking(true);
        m_enabled = true;
        qDebug() << "enabling ActivityDetector";
    }
    if (!enabled && m_enabled) {
        m_mainWindow->setMouseTracking(false);
        m_app->removeEventFilter(this);
        m_enabled = false;
        qDebug() << "disabling ActivityDetector";
    }
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
        setEnabled(false);
    default:;
    }
    return QObject::eventFilter(obj, ev);
}
