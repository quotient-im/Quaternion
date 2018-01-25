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
#include <QtQml> // for qmlRegisterType()

#include "../quaternionroom.h"
#include "lib/connection.h"
#include "lib/user.h"
#include "lib/settings.h"
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
    ReadMarkerRole,
    SpecialMarksRole,
    LongOperationRole,
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
    roles[ReadMarkerRole] = "readMarker";
    roles[SpecialMarksRole] = "marks";
    roles[LongOperationRole] = "progressInfo";
    return roles;
}

MessageEventModel::MessageEventModel(QObject* parent)
    : QAbstractListModel(parent)
    , m_currentRoom(nullptr)
{
    qmlRegisterType<QMatrixClient::FileTransferInfo>();
    qRegisterMetaType<QMatrixClient::FileTransferInfo>();
}

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
                    beginInsertRows(QModelIndex(), 0, int(events.size()) - 1);
                });
        connect(m_currentRoom, &Room::aboutToAddHistoricalMessages, this,
                [=](RoomEventsRange events)
                {
                    beginInsertRows(QModelIndex(), rowCount(),
                                    rowCount() + int(events.size()) - 1);
                });
        connect(m_currentRoom, &Room::addedMessages,
                this, &MessageEventModel::endInsertRows);
        connect(m_currentRoom, &Room::readMarkerMoved, this, [this] {
            refreshEventRoles(
                std::exchange(lastReadEventId,
                              m_currentRoom->readMarkerEventId()),
                {ReadMarkerRole});
            refreshEventRoles(lastReadEventId, {ReadMarkerRole});
        });
        connect(m_currentRoom, &Room::replacedEvent, this,
                [this] (const RoomEvent* newEvent) {
                    refreshEvent(newEvent->id());
                });
        connect(m_currentRoom, &Room::fileTransferProgress,
                this, &MessageEventModel::refreshEvent);
        connect(m_currentRoom, &Room::fileTransferCompleted,
                this, &MessageEventModel::refreshEvent);
        connect(m_currentRoom, &Room::fileTransferFailed,
                this, &MessageEventModel::refreshEvent);
        connect(m_currentRoom, &Room::fileTransferCancelled,
                this, &MessageEventModel::refreshEvent);
        qDebug() << "Connected to room" << room->id()
                 << "as" << room->connection()->userId();
    }
    lastReadEventId = room ? room->readMarkerEventId() : "";
    endResetModel();
}

void MessageEventModel::refreshEvent(const QString& eventId)
{
    refreshEventRoles(eventId, {});
}

void MessageEventModel::refreshEventRoles(const QString& eventId,
                                     const QVector<int> roles)
{
    const auto it = m_currentRoom->findInTimeline(eventId);
    if (it != m_currentRoom->timelineEdge())
    {
        const auto row = it - m_currentRoom->messageEvents().rbegin();
        qDebug() << "Refreshing event" << eventId << " at QML index" << row;
        emit dataChanged(index(row), index(row), roles);
    }
}

inline bool hasValidTimestamp(const QMatrixClient::TimelineItem& ti)
{
    return ti->timestamp().isValid();
}

QDateTime MessageEventModel::makeMessageTimestamp(QuaternionRoom::rev_iter_t baseIt) const
{
    const auto& timeline = m_currentRoom->messageEvents();
    auto ts = baseIt->event()->timestamp();
    if (ts.isValid())
        return ts;

    // The event is most likely redacted or just invalid.
    // Look for the nearest date around and slap zero time to it.
    using QMatrixClient::TimelineItem;
    auto rit = std::find_if(baseIt, timeline.rend(),
                      hasValidTimestamp);
    if (rit != timeline.rend())
        return { rit->event()->timestamp().date(), {0,0}, Qt::LocalTime };
    auto it = std::find_if(baseIt.base(), timeline.end(), hasValidTimestamp);
    if (it != timeline.end())
        return { it->event()->timestamp().date(), {0,0}, Qt::LocalTime };

    // What kind of room is that?..
    qCritical() << "No valid timestamps in the room timeline!";
    return {};
}

QString MessageEventModel::makeDateString(QuaternionRoom::rev_iter_t baseIt) const
{
    auto date = makeMessageTimestamp(baseIt).toLocalTime().date();
    if (QMatrixClient::SettingsGroup("UI")
            .value("banner_human_friendly_date", true).toBool())
    {
        if (date == QDate::currentDate())
            return tr("Today");
        if (date == QDate::currentDate().addDays(-1))
            return tr("Yesterday");
        if (date == QDate::currentDate().addDays(-2))
            return tr("The day before yesterday");
        if (date > QDate::currentDate().addDays(-7))
            return date.toString("dddd");
    }
    return date.toString(Qt::DefaultLocaleShortDate);
}

int MessageEventModel::rowCount(const QModelIndex& parent) const
{
    if( !m_currentRoom || parent.isValid() )
        return 0;
    return m_currentRoom->timelineSize();
}

QVariant MessageEventModel::data(const QModelIndex& index, int role) const
{
    if( !m_currentRoom ||
            index.row() < 0 || index.row() >= m_currentRoom->timelineSize())
        return QVariant();

    const auto eventIt = m_currentRoom->messageEvents().rbegin() + index.row();
    auto* event = eventIt->event();
    // FIXME: Rewind to the name that was right before this event
    QString senderName = m_currentRoom->roomMembername(event->senderId());

    using namespace QMatrixClient;
    if( role == Qt::DisplayRole )
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
            if (e->hasTextContent() && e->mimeType().name() != "text/plain")
                return static_cast<const TextContent*>(e->content())->body;
            if (e->hasFileContent())
            {
                auto fileCaption = e->content()->fileInfo()->originalName;
                if (fileCaption.isEmpty())
                    fileCaption = m_currentRoom->prettyPrint(e->plainBody());
                if (fileCaption.isEmpty())
                    return tr("a file");
            }
            return m_currentRoom->prettyPrint(e->plainBody());
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
                    if (e->prev_content() &&
                            e->prev_content()->membership == MembershipType::Ban)
                    {
                        if (e->senderId() != e->userId())
                            return tr("unbanned %1").arg(subjectName);
                        else
                            return tr("self-unbanned");
                    }
                    if (e->senderId() != e->userId())
                        return tr("has put %1 out of the room").arg(subjectName);
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
        return tr("Unknown Event");
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
                case MessageEventType::Emote:
                    return "emote";
                case MessageEventType::Notice:
                    return "notice";
                case MessageEventType::Image:
                    return "image";
                case MessageEventType::File:
                case MessageEventType::Audio:
                case MessageEventType::Video:
                    return "file";
            default:
                return "message";
            }
        }

        return "other";
    }

    if( role == TimeRole )
        return makeMessageTimestamp(eventIt);

    if( role == SectionRole )
        return makeDateString(eventIt); // FIXME: move date rendering to QML

    if( role == AuthorRole )
    {
        auto userId = event->senderId();
        // FIXME: This will go away after senderName is generated correctly
        // (see the FIXME in the beginning of the method).
        if (event->type() == EventType::RoomMember)
        {
            const auto* e = static_cast<const RoomMemberEvent*>(event);
            if (e->senderId() == e->userId() && e->prev_content()
                    && !e->prev_content()->displayName.isEmpty())
                userId = e->prevSenderId();
        }
        return QVariant::fromValue(m_currentRoom->connection()->user(userId));
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
                case MessageEventType::Image:
                case MessageEventType::File:
                case MessageEventType::Audio:
                case MessageEventType::Video:
                    return QVariant::fromValue(e->content()->originalJson);
            default:
                ;
            }
        }
    }

    if( role == HighlightRole )
        return m_currentRoom->isEventHighlighted(event);

    if( role == ReadMarkerRole )
        return event->id() == lastReadEventId;

    if( role == SpecialMarksRole )
    {
        if (event->isStateEvent() &&
                static_cast<const StateEventBase*>(event)->repeatsState())
            return "hidden";
        return event->isRedacted() ? "redacted" : "";
    }

    if( role == EventIdRole )
        return event->id();

    if( role == LongOperationRole )
    {
        if (event->type() == EventType::RoomMessage &&
                static_cast<const RoomMessageEvent*>(event)->hasFileContent())
        {
            auto info = m_currentRoom->fileTransferInfo(event->id());
            return QVariant::fromValue(info);
        }
    }

    return QVariant();
}
