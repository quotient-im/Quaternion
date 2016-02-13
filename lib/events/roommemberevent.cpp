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

#include "roommemberevent.h"

#include <QtCore/QDebug>
#include <QtCore/QUrl>

using namespace QMatrixClient;

class RoomMemberEvent::Private
{
    public:
        MembershipType membership;
        QString userId;
        QString displayname;
        QUrl avatarUrl;
};

RoomMemberEvent::RoomMemberEvent()
    : Event(EventType::RoomMember)
    , d(new Private)
{
}

RoomMemberEvent::~RoomMemberEvent()
{
    delete d;
}

MembershipType RoomMemberEvent::membership() const
{
    return d->membership;
}

QString RoomMemberEvent::userId() const
{
    return d->userId;
}

QString RoomMemberEvent::displayName() const
{
    return d->displayname;
}

QUrl RoomMemberEvent::avatarUrl() const
{
    return d->avatarUrl;
}

RoomMemberEvent* RoomMemberEvent::fromJson(const QJsonObject& obj)
{
    RoomMemberEvent* e = new RoomMemberEvent();
    e->parseJson(obj);
    e->d->userId = obj.value("state_key").toString();
    QJsonObject content = obj.value("content").toObject();
    e->d->displayname = content.value("displayname").toString();
    QString membershipString = content.value("membership").toString();
    if( membershipString == "invite" )
        e->d->membership = MembershipType::Invite;
    else if( membershipString == "join" )
        e->d->membership = MembershipType::Join;
    else if( membershipString == "knock" )
        e->d->membership = MembershipType::Knock;
    else if( membershipString == "leave" )
        e->d->membership = MembershipType::Leave;
    else if( membershipString == "ban" )
        e->d->membership = MembershipType::Ban;
    else
        qDebug() << "Unknown MembershipType: " << membershipString;
    e->d->avatarUrl = QUrl(content.value("avatar_url").toString());
    return e;
}
