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

#include "room.h"

#include "logmessage.h"

using namespace QMatrixClient;

class Room::Private
{
    public:
        Private() {messages=new QList<LogMessage*>();}

        QList<LogMessage*>* messages;
        QString id;
};

Room::Room(QString id)
    : d(new Private)
{
    d->id = id;
}

Room::~Room()
{
    delete d->messages;
    delete d;
}

QString Room::id() const
{
    return d->id;
}

QList< LogMessage* >* Room::logMessages() const
{
    return d->messages;
}

void Room::addMessages(const QList< LogMessage* >& messages)
{
    for( LogMessage* msg: messages )
    {
        d->messages->append(msg);
    }
}

void Room::addMessage(LogMessage* message)
{
    d->messages->append(message);
}



