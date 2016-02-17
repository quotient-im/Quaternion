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

#include "typingevent.h"

#include <QtCore/QJsonArray>
#include <QtCore/QDebug>

using namespace QMatrixClient;

class TypingEvent::Private
{
    public:
        QStringList users;
};

TypingEvent::TypingEvent()
    : Event(EventType::Typing)
    , d( new Private )
{
}

TypingEvent::~TypingEvent()
{
    delete d;
}

QStringList TypingEvent::users()
{
    return d->users;
}

TypingEvent* TypingEvent::fromJson(const QJsonObject& obj)
{
    TypingEvent* e = new TypingEvent();
    e->parseJson(obj);
    QJsonArray array = obj.value("content").toObject().value("user_ids").toArray();
    for( const QJsonValue& user: array )
    {
        e->d->users << user.toString();
    }
    qDebug() << "TypingEvent in room" << e->roomId() << ":";
    qDebug() << e->d->users;
    return e;
}
