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
#include "events/roomnameevent.h"
#include "events/roomaliasesevent.h"
#include "events/roomcanonicalaliasevent.h"
#include "events/roomtopicevent.h"
#include "events/roommemberevent.h"
#include "events/typingevent.h"
#include "jobs/roommessagesjob.h"

using namespace QMatrixClient;

class Room::Private: public QObject
{
    public:
        Private(Room* parent): q(parent) {}

        Room* q;

        //static LogMessage* parseMessage(const QJsonObject& message);
        void insertMessage(Event* event);
        void addState(Event* event);
        void ephemeralEvent(Event* event);
        void gotMessages(KJob* job);

        Connection* connection;
        QList<Event*> messageEvents;
        QString id;
        QStringList aliases;
        QString canonicalAlias;
        QString name;
        QString topic;
        JoinState joinState;
        QList<User*> users;
        QList<User*> usersTyping;
        QString prevBatch;
        bool gettingNewContent;
};

Room::Room(Connection* connection, QString id)
    : d(new Private(this))
{
    d->id = id;
    d->connection = connection;
    d->joinState = JoinState::Join;
    d->gettingNewContent = false;
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

QString Room::name() const
{
    return d->name;
}

QStringList Room::aliases() const
{
    return d->aliases;
}

QString Room::canonicalAlias() const
{
    return d->canonicalAlias;
}

QString Room::displayName() const
{
    if (name().isEmpty())
    {
        // Without a human name, try to find a substitute
        if (!canonicalAlias().isEmpty())
            return canonicalAlias();
        if (!aliases().empty() && !aliases().at(0).isEmpty())
            return aliases().at(0);
        // Ok, last attempt - one on one chat
        if (users().size() == 2) {
            return users().at(0)->displayname() + " and " +
                    users().at(1)->displayname();
        }
        // Fail miserably
        return id();
    }

    // If we have a non-empty name, try to stack canonical alias to it.
    // The format is unwittingly borrowed from the email address format.
    QString dispName = name();
    if (!canonicalAlias().isEmpty())
        dispName += " <" + canonicalAlias() + ">";

    return dispName;
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

QList< User* > Room::usersTyping() const
{
    return d->usersTyping;
}

QList< User* > Room::users() const
{
    return d->users;
}

void Room::addMessage(Event* event)
{
    d->insertMessage(event);
    emit newMessage(event);
    //d->addState(event);
}

void Room::addInitialState(State* state)
{
    d->addState(state->event());
}

void Room::updateData(const SyncRoomData& data)
{
    if( d->prevBatch.isEmpty() )
        d->prevBatch = data.timelinePrevBatch;
    setJoinState(data.joinState);

    for( Event* stateEvent: data.state )
    {
        d->addState(stateEvent);
    }

    for( Event* timelineEvent: data.timeline )
    {

        d->insertMessage(timelineEvent);
        emit newMessage(timelineEvent);
        // State changes can arrive in a timeline event - try to check those.
        d->addState(timelineEvent);
    }

    for( Event* ephemeralEvent: data.ephemeral )
    {
        d->ephemeralEvent(ephemeralEvent);
    }
}

void Room::getNewContent()
{
    if( !d->gettingNewContent )
    {
        d->gettingNewContent = true;
        RoomMessagesJob* job = d->connection->getMessages(this, d->prevBatch);
        connect( job, &RoomMessagesJob::result, d, &Room::Private::gotMessages );
    }
}

void Room::Private::insertMessage(Event* event)
{
    for( int i=0; i<messageEvents.count(); i++ )
    {
        if( event->timestamp() < messageEvents.at(i)->timestamp() )
        {
            messageEvents.insert(i, event);
            return;
        }
    }
    messageEvents.append(event);
}

void Room::Private::addState(Event* event)
{
    if( event->type() == EventType::RoomName )
    {
        if (RoomNameEvent* nameEvent = static_cast<RoomNameEvent*>(event))
        {
            name = nameEvent->name();
            qDebug() << "room name: " << name;
            emit q->namesChanged(q);
        } else
        {
            qDebug() <<
                "!!! event type is RoomName but the event is not RoomNameEvent";
        }
    }
    if( event->type() == EventType::RoomAliases )
    {
        RoomAliasesEvent* aliasesEvent = static_cast<RoomAliasesEvent*>(event);
        aliases = aliasesEvent->aliases();
        qDebug() << "room aliases: " << aliases;
        emit q->namesChanged(q);
    }
    if( event->type() == EventType::RoomCanonicalAlias )
    {
        RoomCanonicalAliasEvent* aliasEvent = static_cast<RoomCanonicalAliasEvent*>(event);
        canonicalAlias = aliasEvent->alias();
        qDebug() << "room canonical alias: " << canonicalAlias;
        emit q->namesChanged(q);
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
        if (User* u = connection->user(memberEvent->userId()))
        {
            u->processEvent(event);
            if( memberEvent->membership() == MembershipType::Join
                    && !users.contains(u) )
            {
                users.append(u);
                emit q->userAdded(u);
            }
            else if( memberEvent->membership() == MembershipType::Leave
                     && users.contains(u) )
            {
                users.removeAll(u);
                emit q->userRemoved(u);
            }
        }
        else
            qDebug() << "addState: invalid user!" << memberEvent->userId();
    }

}

void Room::Private::ephemeralEvent(Event* event)
{
    if( event->type() == EventType::Typing )
    {
        TypingEvent* typingEvent = static_cast<TypingEvent*>(event);
        usersTyping.clear();
        for( const QString& user: typingEvent->users() )
        {
            usersTyping.append(connection->user(user));
        }
        emit q->typingChanged();
    }
}

void Room::Private::gotMessages(KJob* job)
{
    RoomMessagesJob* realJob = static_cast<RoomMessagesJob*>(job);
    if( realJob->error() )
    {
        qDebug() << realJob->errorString();
    }
    else
    {
        for( Event* event: realJob->events() )
        {
            insertMessage(event);
            emit q->newMessage(event);
        }
        prevBatch = realJob->end();
    }
    gettingNewContent = false;
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

