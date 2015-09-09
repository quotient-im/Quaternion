/******************************************************************************
 * Copyright (C) 2015 Felix Rohrbach <kde@fxrh.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "event.h"

#include <QtCore/QJsonObject>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>

using namespace QMatrixClient;

class Event::Private
{
    public:
        EventType type;
        QString id;
        QDateTime timestamp;
};

Event::Event(EventType type)
    : d(new Private)
{
    d->type = type;
}

EventType Event::type() const
{
    return d->type;
}

QString Event::id() const
{
    return d->id;
}

QDateTime Event::timestamp() const
{
    return d->timestamp;
}

bool Event::parseJson(const QJsonObject& obj)
{
    bool correct = true;
    if( obj.contains("event_id") )
    {
        d->id = obj.value("event_id").toString();
    } else {
        correct = false;
        qDebug() << "Event: can't find event_id";
    }
    if( obj.contains("ts") )
    {
        d->timestamp = QDateTime::fromMSecsSinceEpoch( obj.value("ts").toInt() );
    } else {
        correct = false;
        qDebug() << "Event: can't find ts";
    }
    return correct;
}
