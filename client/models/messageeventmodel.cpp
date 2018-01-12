/**************************************************************************
 *                                                                        *
 * Copyright (C) 2015 Felix Rohrbach <kde@fxrh.de>                        *
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

#include "messageeventmodel.h"

#include <QtCore/QSettings>
#include <QtCore/QDebug>

#include "../quaternionroom.h"
#include "lib/connection.h"
#include "lib/user.h"
#include "lib/events/roommemberevent.h"
#include "lib/events/simplestateevents.h"
#include "lib/events/redactionevent.h"

enum EventRoles {
    EventTypeRole = Qt::UserRole + 1,
    EventIdRole,
    TimeRole,
    SectionRole,
    AuthorRole,
    ContentRole,
    ContentTypeRole,
    HighlightRole,
    SpecialMarksRole,
};

QHash<int, QByteArray> MessageEventModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();
    roles[EventTypeRole] = "eventType";
    roles[EventIdRole] = "eventId";
    roles[TimeRole] = "time";
    roles[SectionRole] = "section";
    roles[AuthorRole] = "author";
    roles[ContentRole] = "content";
    roles[ContentTypeRole] = "contentType";
    roles[HighlightRole] = "highlight";
    roles[SpecialMarksRole] = "marks";
    return roles;
}

MessageEventModel::MessageEventModel(QObject* parent)
    : QAbstractListModel(parent)
    , m_currentRoom(nullptr)
{ }

void MessageEventModel::changeRoom(QuaternionRoom* room)
{
    if (room == m_currentRoom)
        return;

    beginResetModel();
    if( m_currentRoom )
    {
        m_currentRoom->disconnect( this );
        qDebug() << "Disconnected from" << m_currentRoom->id();
    }

    m_currentRoom = room;
    if( room )
    {
        using namespace QMatrixClient;
        connect(m_currentRoom, &Room::aboutToAddNewMessages, this,
                [=](RoomEventsRange events)
                {
                    beginInsertRows(QModelIndex(), rowCount(),
                                    rowCount() + int(events.size()) - 1);
                });
        connect(m_currentRoom, &Room::aboutToAddHistoricalMessages, this,
                [=](RoomEventsRange events)
                {
                    beginInsertRows(QModelIndex(), 0, int(events.size()) - 1);
                });
        connect(m_currentRoom, &Room::addedMessages,
                this, &MessageEventModel::endInsertRows);
        connect(m_currentRoom, &Room::replacedEvent,
                this, &MessageEventModel::refreshEvent);
        qDebug() << "Connected to room" << room->id()
                 << "as" << room->connection()->userId();
    }
    endResetModel();
}

void MessageEventModel::refreshEvent(const QMatrixClient::RoomEvent* event)
{
    // FIXME: Avoid repetetive searching in QuaternionRoom and here.
    // The right way will be to get rid of QuaternionRoom altogether.
    const auto it = std::find_if(
        m_currentRoom->messages().begin(), m_currentRoom->messages().end(),
        [=](const Message& m) { return m.messageEvent() == event; });
    if (it == m_currentRoom->messages().end())
        return;
    const auto row = it - m_currentRoom->messages().begin();
    emit dataChanged(index(row), index(row));
}

QDateTime MessageEventModel::makeMessageTimestamp(int baseIndex) const
{
    Q_ASSERT(m_currentRoom &&
             baseIndex >= 0 && baseIndex < m_currentRoom->messages().count());
    const auto& messages = m_currentRoom->messages();
    auto ts = messages[baseIndex].messageEvent()->timestamp();
    if (ts.isValid())
        return ts;

    // The event is most likely redacted or just invalid.
    // Look for the nearest date around and slap zero time to it.
#if QT_VERSION < QT_VERSION_CHECK(5, 6, 0)
    using rev_iter_t =
        std::reverse_iterator<QuaternionRoom::Timeline::const_iterator>;
    for (auto it = rev_iter_t(messages.begin());
         it != rev_iter_t(messages.begin()); ++it)
#else
    for (auto it = messages.rend() - baseIndex; it != messages.rend(); ++it)
#endif
    {
        ts = it->messageEvent()->timestamp();
        if (ts.isValid())
            return { ts.date(), {0,0}, Qt::LocalTime };
    }
    for (auto it = messages.begin() + baseIndex + 1; it != messages.end(); ++it)
    {
        ts = it->messageEvent()->timestamp();
        if (ts.isValid())
            return { ts.date(), {0,0}, Qt::LocalTime };
    }
    // What kind of room is that?..
    qCritical() << "No valid timestamps in the room timeline!";
    return {};
}

int MessageEventModel::rowCount(const QModelIndex& parent) const
{
    if( !m_currentRoom || parent.isValid() )
        return 0;
    return m_currentRoom->messages().count();
}

QVariant MessageEventModel::data(const QModelIndex& index, int role) const
{
    using namespace QMatrixClient;
    if( !m_currentRoom ||
            index.row() < 0 || index.row() >= m_currentRoom->messages().count())
        return QVariant();

    const Message& message = m_currentRoom->messages().at(index.row());;
    auto* event = message.messageEvent();
    // FIXME: Rewind to the name that was right before this event
    QString senderName = m_currentRoom->roomMembername(event->senderId());

    if( role == Qt::DisplayRole )
    {
        if( event->type() == EventType::RoomMessage )
        {
            auto* e = static_cast<const RoomMessageEvent*>(event);
            return QString("%1: %2").arg(senderName, e->plainBody());
        }
        if( event->type() == EventType::RoomMember )
        {
            auto* e = static_cast<const RoomMemberEvent*>(event);
            switch( e->membership() )
            {
                case MembershipType::Join:
                    return QString("%1 (%2) joined the room").arg(e->displayName(), e->userId());
                case MembershipType::Leave:
                    return QString("%1 (%2) left the room").arg(e->displayName(), e->userId());
                case MembershipType::Ban:
                    return QString("%1 (%2) was banned from the room").arg(e->displayName(), e->userId());
                case MembershipType::Invite:
                    return QString("%1 (%2) was invited to the room").arg(e->displayName(), e->userId());
                case MembershipType::Knock:
                    return QString("%1 (%2) knocked").arg(e->displayName(), e->userId());
            }
        }
        if( event->type() == EventType::RoomAliases )
        {
            auto* e = static_cast<const RoomAliasesEvent*>(event);
            return QString("Current aliases: %1").arg(e->aliases().join(", "));
        }
        return "Unknown Event";
    }

    if( role == Qt::ToolTipRole )
    {
        return event->originalJson();
    }

    if( role == EventTypeRole )
    {
        if (event->isStateEvent())
            return "state";

        if (event->type() == EventType::RoomMessage)
        {
            switch (static_cast<const RoomMessageEvent*>(event)->msgtype())
            {
                case MessageEventType::Image:
                    return "image";
                case MessageEventType::Emote:
                    return "emote";
                case MessageEventType::Notice:
                    return "notice";
            default:
                return "message";
            }
        }

        return "other";
    }

    if( role == TimeRole )
        return makeMessageTimestamp(index.row());

    if( role == SectionRole )
        return makeMessageTimestamp(index.row()).toLocalTime().date();

    if( role == AuthorRole )
    {
        // FIXME: This will go away after senderName is generated correctly
        // (see the FIXME in the beginning of the method).
        if (event->type() == EventType::RoomMember)
        {
            const auto* e = static_cast<const RoomMemberEvent*>(event);
            if (e->prev_content() && !e->prev_content()->displayName.isEmpty()
                    && e->displayName() != e->prev_content()->displayName)
                return e->prev_content()->displayName;
        }
        return senderName;
    }

    if (role == ContentTypeRole)
    {
        if (event->type() == EventType::RoomMessage)
        {
            const auto& contentType =
                static_cast<const RoomMessageEvent*>(event)->mimeType().name();
            return contentType == "text/plain" ? "text/html" : contentType;
        }
        return "text/plain";
    }

    if (role == ContentRole)
    {
        if (event->isRedacted())
        {
            auto reason = event->redactedBecause()->reason();
            if (reason.isEmpty())
                return tr("Redacted");
            else
                return tr("Redacted: %1")
                    .arg(event->redactedBecause()->reason());
        }

        if( event->type() == EventType::RoomMessage )
        {
            using namespace MessageEventContent;

            auto* e = static_cast<const RoomMessageEvent*>(event);
            switch (e->msgtype())
            {
            case MessageEventType::Emote:
            case MessageEventType::Text:
            case MessageEventType::Notice:
                {
                    if (e->mimeType().name() == "text/plain")
                        return m_currentRoom->prettyPrint(e->plainBody());

                    return static_cast<const TextContent*>(e->content())->body;
                }
            case MessageEventType::Image:
                {
                    auto content = static_cast<const ImageContent*>(e->content());
                    return QUrl("image://mtx/" +
                                content->url.host() + content->url.path());
                }
            default:
                return e->plainBody();
            }
        }
        if( event->type() == EventType::RoomMember )
        {
            auto* e = static_cast<const RoomMemberEvent*>(event);
            // FIXME: Rewind to the name that was at the time of this event
            QString subjectName = m_currentRoom->roomMembername(e->userId());
            // The below code assumes senderName output in AuthorRole
            switch( e->membership() )
            {
                case MembershipType::Invite:
                    if (e->repeatsState())
                        return tr("reinvited %1 to the room").arg(subjectName);
                    // [[fallthrough]]
                case MembershipType::Join:
                {
                    if (e->repeatsState())
                        return tr("joined the room (repeated)");
                    if (!e->prev_content() ||
                            e->membership() != e->prev_content()->membership)
                    {
                        return e->membership() == MembershipType::Invite
                                ? tr("invited %1 to the room").arg(subjectName)
                                : tr("joined the room");
                    }
                    QString text {};
                    if (e->displayName() != e->prev_content()->displayName)
                    {
                        if (e->displayName().isEmpty())
                            text = tr("cleared the display name");
                        else
                            text = tr("changed the display name to %1")
                                        .arg(e->displayName());
                    }
                    if (e->avatarUrl() != e->prev_content()->avatarUrl)
                    {
                        if (!text.isEmpty())
                            text += " and ";
                        if (e->avatarUrl().isEmpty())
                            text += tr("cleared the avatar");
                        else
                            text += tr("updated the avatar");
                    }
                    return text;
                }
                case MembershipType::Leave:
                    if (e->senderId() != e->userId())
                        return tr("doesn't want %1 in the room anymore").arg(subjectName);
                    else
                        return tr("left the room");
                case MembershipType::Ban:
                    if (e->senderId() != e->userId())
                        return tr("banned %1 from the room").arg(subjectName);
                    else
                        return tr("self-banned from the room");
                case MembershipType::Knock:
                    return tr("knocked");
            }
        }
        if( event->type() == EventType::RoomAliases )
        {
            auto* e = static_cast<const RoomAliasesEvent*>(event);
            return tr("set aliases to: %1").arg(e->aliases().join(", "));
        }
        if( event->type() == EventType::RoomCanonicalAlias )
        {
            auto* e = static_cast<const RoomCanonicalAliasEvent*>(event);
            return tr("set the room main alias to: %1").arg(e->alias());
        }
        if( event->type() == EventType::RoomName )
        {
            auto* e = static_cast<const RoomNameEvent*>(event);
            return tr("set the room name to: %1").arg(e->name());
        }
        if( event->type() == EventType::RoomTopic )
        {
            auto* e = static_cast<const RoomTopicEvent*>(event);
            return tr("set the topic to: %1").arg(e->topic());
        }
        if( event->type() == EventType::RoomAvatar )
        {
            return tr("changed the room avatar");
        }
        if( event->type() == EventType::RoomEncryption )
        {
            return tr("activated End-to-End Encryption");
        }
        return "Unknown Event";
    }

    if( role == HighlightRole )
    {
        return message.highlight();
    }

    if( role == SpecialMarksRole )
    {
        auto* e = message.messageEvent();
        if (e->isStateEvent() &&
                static_cast<const StateEventBase*>(e)->repeatsState())
            return "hidden";
        return e->isRedacted() ? "redacted" : "";
    }

    if( role == Qt::DecorationRole )
    {
        if (message.highlight())
        {
            return QSettings().value("UI/highlight_color", "orange");
        }
    }
    if( role == EventIdRole )
    {
        return event->id();
    }

    return QVariant();
}
