/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2016 Malte Brandy <malte.brandy@maralorn.de>   *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#include "activitydetector.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QtCore/QDebug>

void ActivityDetector::setEnabled(bool enabled)
{
    if (enabled == m_enabled)
        return;

    m_enabled = enabled;
    const auto& topLevels = qApp->topLevelWidgets();
    for (auto* w: topLevels)
        if (!w->isHidden())
            w->setMouseTracking(enabled);
    if (enabled)
        qApp->installEventFilter(this);
    else
        qApp->removeEventFilter(this);
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
