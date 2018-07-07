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
#include "lib/events/roomavatarevent.h"

enum EventRoles {
    EventTypeRole = Qt::UserRole + 1,
    EventIdRole,
    TimeRole,
    SectionRole,
    AboveSectionRole,
    AuthorRole,
    AboveAuthorRole,
    ContentRole,
    ContentTypeRole,
    HighlightRole,
    ReadMarkerRole,
    SpecialMarksRole,
    LongOperationRole,
    // For debugging
    EventResolvedTypeRole,
};

QHash<int, QByteArray> MessageEventModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();
    roles[EventTypeRole] = "eventType";
    roles[EventIdRole] = "eventId";
    roles[TimeRole] = "time";
    roles[SectionRole] = "section";
    roles[AboveSectionRole] = "aboveSection";
    roles[AuthorRole] = "author";
    roles[AboveAuthorRole] = "aboveAuthor";
    roles[ContentRole] = "content";
    roles[ContentTypeRole] = "contentType";
    roles[HighlightRole] = "highlight";
    roles[ReadMarkerRole] = "readMarker";
    roles[SpecialMarksRole] = "marks";
    roles[LongOperationRole] = "progressInfo";
    roles[EventResolvedTypeRole] = "eventResolvedType";
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
        lastReadEventId = room->readMarkerEventId();
        using namespace QMatrixClient;
        connect(m_currentRoom, &Room::aboutToAddNewMessages, this,
                [=](RoomEventsRange events)
                {
                    beginInsertRows(QModelIndex(), 0, int(events.size()) - 1);
                });
        connect(m_currentRoom, &Room::aboutToAddHistoricalMessages, this,
                [=](RoomEventsRange events)
                {
                    if (rowCount() > 0)
                        nextNewerRow = rowCount() - 1;
                    beginInsertRows(QModelIndex(), rowCount(),
                                    rowCount() + int(events.size()) - 1);
                });
        connect(m_currentRoom, &Room::addedMessages, this,
                [=] {
                    if (nextNewerRow > -1)
                    {
                        const auto idx = index(nextNewerRow);
                        emit dataChanged(idx, idx);
                        nextNewerRow = -1;
                    }
                    endInsertRows();
                });
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
                 << "as" << room->localUser()->id();
    } else
        lastReadEventId.clear();
    endResetModel();
}

void MessageEventModel::refreshEvent(const QString& eventId)
{
    refreshEventRoles(eventId, {});
}

void MessageEventModel::refreshEventRoles(const QString& eventId,
                                     const QVector<int>& roles)
{
    const auto it = m_currentRoom->findInTimeline(eventId);
    if (it != m_currentRoom->timelineEdge())
    {
        const auto idx = index(it - m_currentRoom->messageEvents().rbegin());
        emit dataChanged(idx, idx, roles);
    }
}

inline bool hasValidTimestamp(const QMatrixClient::TimelineItem& ti)
{
    return ti->timestamp().isValid();
}

QDateTime MessageEventModel::makeMessageTimestamp(
            const QuaternionRoom::rev_iter_t& baseIt) const
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

QString MessageEventModel::makeDateString(
            const QuaternionRoom::rev_iter_t& baseIt) const
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

    const auto timelineIt = m_currentRoom->messageEvents().rbegin() + index.row();
    const auto& ti = *timelineIt;

    using namespace QMatrixClient;
    if( role == Qt::DisplayRole )
    {
        if (ti->isRedacted())
        {
            auto reason = ti->redactedBecause()->reason();
            if (reason.isEmpty())
                return tr("Redacted");

            return tr("Redacted: %1")
                .arg(ti->redactedBecause()->reason());
        }

        return visit(*ti
            , [this] (const RoomMessageEvent& e) -> QVariant {
                using namespace MessageEventContent;

                if (e.hasTextContent() && e.mimeType().name() != "text/plain")
                    return static_cast<const TextContent*>(e.content())->body;
                if (e.hasFileContent())
                {
                    auto fileCaption = e.content()->fileInfo()->originalName;
                    if (fileCaption.isEmpty())
                        fileCaption = m_currentRoom->prettyPrint(e.plainBody());
                    if (fileCaption.isEmpty())
                        return tr("a file");
                }
                return m_currentRoom->prettyPrint(e.plainBody());
            }
            , [this] (const RoomMemberEvent& e) -> QVariant {
                // FIXME: Rewind to the name that was at the time of this event
                QString subjectName = m_currentRoom->roomMembername(e.userId());
                // The below code assumes senderName output in AuthorRole
                switch( e.membership() )
                {
                    case MembershipType::Invite:
                        if (e.repeatsState())
                            return tr("reinvited %1 to the room").arg(subjectName);
                        // [[fallthrough]]
                    case MembershipType::Join:
                    {
                        if (e.repeatsState())
                            return tr("joined the room (repeated)");
                        if (!e.prevContent() ||
                                e.membership() != e.prevContent()->membership)
                        {
                            return e.membership() == MembershipType::Invite
                                    ? tr("invited %1 to the room").arg(subjectName)
                                    : tr("joined the room");
                        }
                        QString text {};
                        if (e.displayName() != e.prevContent()->displayName)
                        {
                            if (e.displayName().isEmpty())
                                text = tr("cleared the display name");
                            else
                                text = tr("changed the display name to %1")
                                            .arg(e.displayName());
                        }
                        if (e.avatarUrl() != e.prevContent()->avatarUrl)
                        {
                            if (!text.isEmpty())
                                text += " and ";
                            if (e.avatarUrl().isEmpty())
                                text += tr("cleared the avatar");
                            else
                                text += tr("updated the avatar");
                        }
                        return text;
                    }
                    case MembershipType::Leave:
                        if (e.prevContent() &&
                                e.prevContent()->membership == MembershipType::Ban)
                        {
                            return (e.senderId() != e.userId())
                                    ? tr("unbanned %1").arg(subjectName)
                                    : tr("self-unbanned");
                        }
                        return (e.senderId() != e.userId())
                                ? tr("has put %1 out of the room").arg(subjectName)
                                : tr("left the room");
                    case MembershipType::Ban:
                        return (e.senderId() != e.userId())
                                ? tr("banned %1 from the room").arg(subjectName)
                                : tr("self-banned from the room");
                    case MembershipType::Knock:
                        return tr("knocked");
                    case MembershipType::Undefined:
                        return tr("made something unknown");
                }
            }
            , [] (const RoomAliasesEvent& e) -> QVariant {
                return tr("set aliases to: %1").arg(e.aliases().join(", "));
            }
            , [] (const RoomCanonicalAliasEvent& e) -> QVariant {
                return (e.alias().isEmpty())
                        ? tr("cleared the room main alias")
                        : tr("set the room main alias to: %1").arg(e.alias());
            }
            , [] (const RoomNameEvent& e) -> QVariant {
                return (e.name().isEmpty())
                        ? tr("cleared the room name")
                        : tr("set the room name to: %1").arg(e.name());
            }
            , [] (const RoomTopicEvent& e) -> QVariant {
                return (e.topic().isEmpty())
                        ? tr("cleared the topic")
                        : tr("set the topic to: %1").arg(e.topic());
            }
            , [] (const RoomAvatarEvent&) -> QVariant {
                return tr("changed the room avatar");
            }
            , [] (const EncryptionEvent&) -> QVariant {
                return tr("activated End-to-End Encryption");
            }
            , tr("Unknown Event")
        );
    }

    if( role == Qt::ToolTipRole )
    {
        return ti->originalJson();
    }

    if( role == EventTypeRole )
    {
        if (ti->isStateEvent())
            return "state";

        if (is<RoomMessageEvent>(*ti))
        {
            switch (ti.viewAs<RoomMessageEvent>()->msgtype())
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

    if (role == EventResolvedTypeRole)
        return EventTypeRegistry::getMatrixType(ti->type());

    if( role == TimeRole )
        return makeMessageTimestamp(timelineIt);

    if( role == SectionRole )
        return makeDateString(timelineIt); // FIXME: move date rendering to QML

    if( role == AuthorRole )
    {
        auto userId = ti->senderId();
        // FIXME: It shouldn't be User, it should be its state "as of event"
        return QVariant::fromValue(m_currentRoom->user(userId));
    }

    if (role == ContentTypeRole)
    {
        if (is<RoomMessageEvent>(*ti))
        {
            const auto& contentType =
                ti.viewAs<RoomMessageEvent>()->mimeType().name();
            return contentType == "text/plain" ? "text/html" : contentType;
        }
        return "text/plain";
    }

    if (role == ContentRole)
    {
        if (ti->isRedacted())
        {
            auto reason = ti->redactedBecause()->reason();
            return (reason.isEmpty())
                    ? tr("Redacted")
                    : tr("Redacted: %1").arg(ti->redactedBecause()->reason());
        }

        if (is<RoomMessageEvent>(*ti))
        {
            using namespace MessageEventContent;

            auto* e = ti.viewAs<RoomMessageEvent>();
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
        return m_currentRoom->isEventHighlighted(ti.event());

    if( role == ReadMarkerRole )
        return ti->id() == lastReadEventId;

    if( role == SpecialMarksRole )
    {
        if (ti->isStateEvent() && ti.viewAs<StateEventBase>()->repeatsState())
            return "hidden";
        return ti->isRedacted() ? "redacted" : "";
    }

    if( role == EventIdRole )
        return ti->id();

    if( role == LongOperationRole )
    {
        if (is<RoomMessageEvent>(*ti) &&
                ti.viewAs<RoomMessageEvent>()->hasFileContent())
        {
            auto info = m_currentRoom->fileTransferInfo(ti->id());
            return QVariant::fromValue(info);
        }
    }

    auto aboveEventIt = timelineIt + 1; // FIXME: shouldn't be here, because #312
    if (aboveEventIt != m_currentRoom->timelineEdge())
    {
        if( role == AboveSectionRole )
            return makeDateString(aboveEventIt);

        if( role == AboveAuthorRole )
            return QVariant::fromValue(
                        m_currentRoom->user((*aboveEventIt)->senderId()));
    }

    return QVariant();
}
