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

#include "user.h"

#include "events/event.h"
#include "events/roommemberevent.h"

using namespace QMatrixClient;

class User::Private
{
    public:
        QString userId;
        QString displayname;
};

User::User(QString userId)
    : d(new Private)
{
    d->userId = userId;
}

User::~User()
{
    delete d;
}

QString User::id() const
{
    return d->userId;
}

QString User::displayname() const
{
    return d->displayname;
}

void User::processEvent(Event* event)
{
    if( event->type() == EventType::RoomMember )
    {
        RoomMemberEvent* e = static_cast<RoomMemberEvent*>(event);
        if( d->displayname != e->displayName() )
        {
            d->displayname = e->displayName();
            emit displaynameChanged();
        }
    }
}