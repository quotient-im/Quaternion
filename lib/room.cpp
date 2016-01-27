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

#include <QtCore/QJsonArray>
#include <QtCore/QDebug>

#include "connection.h"
#include "state.h"
#include "user.h"
#include "events/event.h"
#include "events/roommessageevent.h"
#include "events/roomaliasesevent.h"
#include "events/roomtopicevent.h"
#include "events/roommemberevent.h"

using namespace QMatrixClient;

class Room::Private
{
    public:
        Private(Room* parent): q(parent) {}

        Room* q;

        //static LogMessage* parseMessage(const QJsonObject& message);
        void addState(Event* event);

        Connection* connection;
        QList<Event*> messageEvents;
        QString id;
        QString alias;
        QString topic;
        JoinState joinState;
        QList<User*> users;
};

Room::Room(Connection* connection, QString id)
    : d(new Private(this))
{
    d->id = id;
    d->connection = connection;
    d->alias = id;
    d->joinState = JoinState::Join;
    qDebug() << "New Room: " << id;

    //connection->getMembers(this); // I don't think we need this anymore in r0.0.1
}

Room::~Room()
{
    delete d;
}

QString Room::id() const
{
    return d->id;
}

QList< Event* > Room::messages() const
{
    return d->messageEvents;
}

QString Room::alias() const
{
    return d->alias;
}

QString Room::topic() const
{
    return d->topic;
}

JoinState Room::joinState() const
{
    return d->joinState;
}

void Room::setJoinState(JoinState state)
{
    JoinState oldState = d->joinState;
    if( state == oldState )
        return;
    d->joinState = state;
    emit joinStateChanged(oldState, state);
}

QList< User* > Room::users() const
{
    return d->users;
}

void Room::addMessage(Event* event)
{
    d->messageEvents.append(event);
    emit newMessage(event);
    d->addState(event);
}

void Room::addInitialState(State* state)
{
    d->addState(state->event());
}

void Room::updateData(const SyncRoomData& data)
{
    setJoinState(data.joinState);

    for( Event* stateEvent: data.state )
    {
        d->addState(stateEvent);
    }

    for( Event* timelineEvent: data.timeline )
    {
        d->messageEvents.append(timelineEvent);
        emit newMessage(timelineEvent);
    }

    for( Event* ephemeralEvent: data.ephemeral )
    {
        // TODO
    }
}

void Room::Private::addState(Event* event)
{
    if( event->type() == EventType::RoomAliases )
    {
        RoomAliasesEvent* aliasesEvent = static_cast<RoomAliasesEvent*>(event);
        if( aliasesEvent->aliases().count() > 0 )
        {
            alias = aliasesEvent->aliases().first();
            emit q->aliasChanged(q);
        }
    }
    if( event->type() == EventType::RoomTopic )
    {
        RoomTopicEvent* topicEvent = static_cast<RoomTopicEvent*>(event);
        topic = topicEvent->topic();
        emit q->topicChanged();
    }
    if( event->type() == EventType::RoomMember )
    {
        RoomMemberEvent* memberEvent = static_cast<RoomMemberEvent*>(event);
        User* u = connection->user(memberEvent->userId());
        if( !u ) qDebug() << "addState: invalid user!" << u << memberEvent->userId();
        u->processEvent(event);
        if( memberEvent->membership() == MembershipType::Join and !users.contains(u) )
        {
            users.append(u);
            emit q->userAdded(u);
        }
        else if( memberEvent->membership() == MembershipType::Leave
                 and users.contains(u) )
        {
            users.removeAll(u);
            emit q->userRemoved(u);
        }
    }
}

// void Room::setAlias(QString alias)
// {
//     d->alias = alias;
//     emit aliasChanged(this);
// }
//
// bool Room::parseEvents(const QJsonObject& json)
// {
//     QList<LogMessage*> newMessages;
//     QJsonValue value = json.value("messages").toObject().value("chunk");
//     if( !value.isArray() )
//     {
//         return false;
//     }
//     QJsonArray messages = value.toArray();
//     for(const QJsonValue& val: messages )
//     {
//         if( !val.isObject() )
//             continue;
//         LogMessage* msg = Private::parseMessage(val.toObject());
//         if( msg )
//         {
//             newMessages.append(msg);
//         }
//
//     }
//     addMessages(newMessages);
//     return true;
// }
//
// bool Room::parseSingleEvent(const QJsonObject& json)
// {
//     qDebug() << "parseSingleEvent";
//     LogMessage* msg = Private::parseMessage(json);
//     if( msg )
//     {
//         addMessage(msg);
//         return true;
//     }
//     return false;
// }
//
// bool Room::parseState(const QJsonObject& json)
// {
//     QJsonValue value = json.value("state");
//     if( !value.isArray() )
//     {
//         return false;
//     }
//     QJsonArray states = value.toArray();
//     for( const QJsonValue& val: states )
//     {
//         QJsonObject state = val.toObject();
//         QString type = state.value("type").toString();
//         if( type == "m.room.aliases" )
//         {
//             QJsonArray aliases = state.value("content").toObject().value("aliases").toArray();
//             if( aliases.count() > 0 )
//             {
//                 setAlias(aliases.at(0).toString());
//             }
//         }
//     }
//     return true;
// }
//
// LogMessage* Room::Private::parseMessage(const QJsonObject& message)
// {
//     if( message.value("type") == "m.room.message" )
//     {
//         QJsonObject content = message.value("content").toObject();
//         if( content.value("msgtype").toString() != "m.text" )
//             return 0;
//         QString user = message.value("user_id").toString();
//         QString body = content.value("body").toString();
//         LogMessage* msg = new LogMessage( LogMessage::UserMessage, body, user );
//         return msg;
//     }
//     return 0;
// }

