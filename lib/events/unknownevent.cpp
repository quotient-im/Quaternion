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

#include "unknownevent.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QDebug>

using namespace QMatrixClient;

class UnknownEvent::Private
{
    public:
        QString type;
        QString content;
};

UnknownEvent::UnknownEvent()
    : Event(EventType::Unknown)
    , d(new Private)
{
}

UnknownEvent::~UnknownEvent()
{
    delete d;
}

QString UnknownEvent::typeString() const
{
    return d->type;
}

QString UnknownEvent::content() const
{
    return d->content;
}

UnknownEvent* UnknownEvent::fromJson(const QJsonObject& obj)
{
    UnknownEvent* e = new UnknownEvent();
    e->parseJson(obj);
    e->d->type = obj.value("type").toString();
    e->d->content = QString::fromUtf8(QJsonDocument(obj).toJson());
    qDebug() << "UnknownEvent, JSON follows:";
    qDebug().noquote() << obj;
    return e;
}
