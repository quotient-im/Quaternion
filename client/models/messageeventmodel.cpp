/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2015 Felix Rohrbach <kde@fxrh.de>                        *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#include "messageeventmodel.h"

#include <QtCore/QDebug>
#include <QtGui/QPalette>
#include <QtQml> // for qmlRegisterType()

#include "../quaternionroom.h"
#include "../htmlfilter.h"
#include <connection.h>
#include <user.h>
#include <settings.h>
#include <uri.h>
#include <events/encryptionevent.h>
#include <events/roommemberevent.h>
#include <events/simplestateevents.h>
#include <events/redactionevent.h>
#include <events/roomavatarevent.h>
#include <events/roomcreateevent.h>
#include <events/roomtombstoneevent.h>
#include <events/roomcanonicalaliasevent.h>
#include <events/reactionevent.h>

QHash<int, QByteArray> MessageEventModel::roleNames() const
{
    static const auto roles = [this] {
        auto roles = QAbstractItemModel::roleNames();
        // Not every Qt standard role has a role name, turns out
        roles.insert(Qt::ForegroundRole, "foreground");
        roles.insert(EventTypeRole, "eventType");
        roles.insert(EventIdRole, "eventId");
        roles.insert(TimeRole, "time");
        roles.insert(DateRole, "date");
        roles.insert(EventGroupingRole, "eventGrouping");
        roles.insert(AuthorRole, "author");
        roles.insert(ContentRole, "content");
        roles.insert(ContentTypeRole, "contentType");
        roles.insert(HighlightRole, "highlight");
        roles.insert(SpecialMarksRole, "marks");
        roles.insert(LongOperationRole, "progressInfo");
        roles.insert(AnnotationRole, "annotation");
        roles.insert(EventResolvedTypeRole, "eventResolvedType");
        roles.insert(RefRole, "refId");
        roles.insert(ReactionsRole, "reactions");
        roles.insert(BareRichBodyRole, "bareRichBody");
        roles.insert(QuotationRole, "quotation");
        roles.insert(HtmlQuotationRole, "htmlQuotation");
        return roles;
    }();
    return roles;
}

MessageEventModel::MessageEventModel(QObject* parent)
    : QAbstractListModel(parent)
{
    using namespace Quotient;
    qmlRegisterAnonymousType<FileTransferInfo>("Quotient", 1);
    qmlRegisterUncreatableMetaObject(EventStatus::staticMetaObject,
                                     "Quotient", 1, 0, "EventStatus",
                                     "Access to EventStatus enums only");
    qmlRegisterUncreatableMetaObject(EventGrouping::staticMetaObject,
                                     "Quotient", 1, 0, "EventGrouping",
                                     "Access to enums only");
    // This could be a single line in changeRoom() but then there's a race
    // condition between the model reset completion and the room property
    // update in QML - connecting the two signals early on overtakes any QML
    // connection to modelReset. Ideally the room property could use modelReset
    // for its NOTIFY signal - unfortunately, moc doesn't support using
    // parent's signals with parameters in NOTIFY
    connect(this, &MessageEventModel::modelReset, //
            this, &MessageEventModel::roomChanged);
}

QuaternionRoom* MessageEventModel::room() const { return m_currentRoom; }

void MessageEventModel::changeRoom(QuaternionRoom* room)
{
    if (room == m_currentRoom)
        return;

    if (m_currentRoom)
        qDebug() << "Disconnecting event model from"
                 << m_currentRoom->objectName();
    beginResetModel();
    if (m_currentRoom)
        m_currentRoom->disconnect(this);

    m_currentRoom = room;
    if (m_currentRoom) {
        using namespace Quotient;
        connect(m_currentRoom, &Room::aboutToAddNewMessages, this,
                [this](RoomEventsRange events) {
                    incomingEvents(events, timelineBaseIndex());
                });
        connect(m_currentRoom, &Room::aboutToAddHistoricalMessages, this,
                [this](RoomEventsRange events) {
                    incomingEvents(events, rowCount());
                });
        connect(m_currentRoom, &Room::addedMessages, this,
                [this] (int lowest, int biggest) {
                    endInsertRows();
                    if (biggest < m_currentRoom->maxTimelineIndex())
                    {
                        const auto rowBelowInserted =
                            m_currentRoom->maxTimelineIndex() - biggest
                            + timelineBaseIndex() - 1;
                        refreshEventRoles(rowBelowInserted,
                                          { EventGroupingRole });
                    }
                    for (auto i = m_currentRoom->maxTimelineIndex() - biggest;
                              i <= m_currentRoom->maxTimelineIndex() - lowest;
                              ++i)
                        refreshLastUserEvents(i);
                });
        connect(m_currentRoom, &Room::pendingEventAboutToAdd, this,
                [this] { beginInsertRows({}, 0, 0); });
        connect(m_currentRoom, &Room::pendingEventAdded,
                this, &MessageEventModel::endInsertRows);
        connect(m_currentRoom, &Room::pendingEventAboutToMerge, this,
                [this] (RoomEvent*, int i)
                {
                    if (i == 0)
                        return; // No need to move anything, just refresh

                    movingEvent = true;
                    // Reverse i because row 0 is bottommost in the model
                    const auto row = timelineBaseIndex() - i - 1;
                    auto moveBegan = beginMoveRows({}, row, row,
                                                   {}, timelineBaseIndex());
                    Q_ASSERT(moveBegan);
                });
        connect(m_currentRoom, &Room::pendingEventMerged, this,
                [this] {
                    if (movingEvent)
                    {
                        endMoveRows();
                        movingEvent = false;
                    }
                    refreshRow(timelineBaseIndex()); // Refresh the looks
                    refreshLastUserEvents(0);
                    if (timelineBaseIndex() > 0) // Refresh below, see #312
                        refreshEventRoles(timelineBaseIndex() - 1,
                                          { EventGroupingRole });
                });
        connect(m_currentRoom, &Room::pendingEventChanged,
                this, &MessageEventModel::refreshRow);
        connect(m_currentRoom, &Room::pendingEventAboutToDiscard,
                this, [this] (int i) { beginRemoveRows({}, i, i); });
        connect(m_currentRoom, &Room::pendingEventDiscarded,
                this, &MessageEventModel::endRemoveRows);
        connect(m_currentRoom, &Room::readMarkerMoved,
                this, &MessageEventModel::readMarkerUpdated);
        connect(m_currentRoom, &Room::replacedEvent, this,
                [this] (const RoomEvent* newEvent) {
                    refreshLastUserEvents(
                        refreshEvent(newEvent->id()) - timelineBaseIndex());
                });
        connect(m_currentRoom, &Room::updatedEvent,
                this, &MessageEventModel::refreshEvent);
        connect(m_currentRoom, &Room::fileTransferProgress,
                this, &MessageEventModel::refreshEvent);
        connect(m_currentRoom, &Room::fileTransferCompleted,
                this, &MessageEventModel::refreshEvent);
        connect(m_currentRoom, &Room::fileTransferFailed,
                this, &MessageEventModel::refreshEvent);
        qDebug() << "Event model connected to room" << room->objectName()
                 << "as" << room->localUser()->id();
    }
    endResetModel();
    emit readMarkerUpdated();
}

int MessageEventModel::refreshEvent(const QString& eventId)
{
    int row = findRow(eventId, true);
    if (row >= 0)
        refreshEventRoles(row);
    else
        qWarning() << "Trying to refresh inexistent event:" << eventId;
    return row;
}

void MessageEventModel::refreshRow(int row)
{
    refreshEventRoles(row);
}

void MessageEventModel::incomingEvents(Quotient::RoomEventsRange events,
                                       int atIndex)
{
    beginInsertRows({}, atIndex, atIndex + int(events.size()) - 1);
}

int MessageEventModel::readMarkerVisualIndex() const
{
    if (!m_currentRoom)
        return -1; // Beyond the bottommost (sync) edge of the timeline
    if (auto r = findRow(m_currentRoom->lastFullyReadEventId()); r != -1) {
        // Ensure that the read marker is on a visible event
        // TODO: move this to libQuotient once it allows to customise
        //       event status calculation
        while (r < rowCount() - 1
               && data(index(r, 0), SpecialMarksRole)
                  == Quotient::EventStatus::Hidden)
            ++r;
        return r;
    }
    return rowCount(); // Beyond the topmost (history) edge of the timeline
}

int MessageEventModel::timelineBaseIndex() const
{
    return m_currentRoom ? int(m_currentRoom->pendingEvents().size()) : 0;
}

void MessageEventModel::refreshEventRoles(int row, const QVector<int>& roles)
{
    const auto idx = index(row);
    emit dataChanged(idx, idx, roles);
}

int MessageEventModel::findRow(const QString& id, bool includePending) const
{
    // On 64-bit platforms, difference_type for std containers is long long
    // but Qt uses int throughout its interfaces; hence casting to int below.
    if (!id.isEmpty()) {
        // First try pendingEvents because it is almost always very short.
        if (includePending) {
            const auto pendingIt = m_currentRoom->findPendingEvent(id);
            if (pendingIt != m_currentRoom->pendingEvents().end())
                return int(pendingIt - m_currentRoom->pendingEvents().begin());
        }
        const auto timelineIt = m_currentRoom->findInTimeline(id);
        if (timelineIt != m_currentRoom->historyEdge())
            return int(timelineIt - m_currentRoom->messageEvents().rbegin())
                    + timelineBaseIndex();
    }
    return -1;
}

inline bool hasValidTimestamp(const Quotient::TimelineItem& ti)
{
    return ti->originTimestamp().isValid();
}

QDateTime MessageEventModel::makeMessageTimestamp(
            const QuaternionRoom::rev_iter_t& baseIt) const
{
    const auto& timeline = m_currentRoom->messageEvents();
    if (auto ts = baseIt->event()->originTimestamp(); ts.isValid())
        return ts;

    // The event is most likely redacted or just invalid.
    // Look for the nearest date around and slap zero time to it.
    if (auto rit = std::find_if(baseIt, timeline.rend(), hasValidTimestamp);
            rit != timeline.rend())
        return { rit->event()->originTimestamp().date(), {0,0}, Qt::LocalTime };
    if (auto it = std::find_if(baseIt.base(), timeline.end(), hasValidTimestamp);
            it != timeline.end())
        return { it->event()->originTimestamp().date(), {0,0}, Qt::LocalTime };

    // What kind of room is that?..
    qCritical() << "No valid timestamps in the room timeline!";
    return {};
}

QString MessageEventModel::renderDate(const QDateTime& timestamp)
{
    auto date = timestamp.toLocalTime().date();
    static Quotient::SettingsGroup sg { "UI" };
    if (sg.get("use_human_friendly_dates",
               sg.get("banner_human_friendly_date", true)))
    {
        if (date == QDate::currentDate())
            return tr("Today");
        if (date == QDate::currentDate().addDays(-1))
            return tr("Yesterday");
        if (date == QDate::currentDate().addDays(-2))
            return tr("The day before yesterday");
        if (date > QDate::currentDate().addDays(-7))
        {
            auto s = QLocale().standaloneDayName(date.dayOfWeek());
            // Some locales (e.g., Russian on Windows) don't capitalise
            // the day name so make sure the first letter is uppercase.
            if (!s.isEmpty() && !s[0].isUpper())
                s[0] = QLocale().toUpper(s.mid(0,1)).at(0);
            return s;
        }
    }
    return QLocale().toString(date, QLocale::ShortFormat);
}

bool MessageEventModel::isUserActivityNotable(
        const QuaternionRoom::rev_iter_t& baseIt) const
{
    const auto& userId = (*baseIt)->isStateEvent()
                            ? (*baseIt)->stateKey() : (*baseIt)->senderId();

    // Go up to the nearest join and down to the nearest leave of this author
    // (limit the lookup to 100 events for the sake of performance);
    // in this range find out if there's any event from that user besides
    // joins, leaves and redacted (self- or by somebody else); if there's not,
    // double-check that there are no redactions and that it's not a single
    // join or leave.

    using namespace Quotient;
    bool joinFound = false, redactionsFound = false;
    // Find the nearest join of this user above, or a no-nonsense event.
    for (auto it = baseIt,
              limit = baseIt +
                    std::min(int(m_currentRoom->historyEdge() - baseIt), 100);
         it != limit; ++it)
    {
        const auto& e = **it;
        if (e.senderId() != userId && e.stateKey() != userId)
            continue;

        if (e.isRedacted())
        {
            redactionsFound = true;
            continue;
        }

        if (auto* me = it->viewAs<RoomMemberEvent>())
        {
            if (e.stateKey() != userId)
                return true; // An action on another member is notable
            if (!me->isJoin())
                continue;

            joinFound = true;
            break;
        }
        return true; // Consider all other events notable
    }

    // Find the nearest leave of this user below, or a no-nonsense event
    bool leaveFound = false;
    for (auto it = baseIt.base() - 1,
              limit = baseIt.base() +
                std::min(int(m_currentRoom->messageEvents().end() - baseIt.base()),
                         100);
         it != limit; ++it)
    {
        const auto& e = **it;
        if (e.senderId() != userId && e.stateKey() != userId)
            continue;

        if (e.isRedacted())
        {
            redactionsFound = true;
            continue;
        }

        if (auto* me = it->viewAs<RoomMemberEvent>())
        {
            if (e.stateKey() != userId)
                return true; // An action on another member is notable
            if (!me->isLeave() && me->membership() != Membership::Ban)
                continue;

            leaveFound = true;
            break;
        }
        return true;
    }
    // If we are here, it means that no notable events have been found in
    // the timeline vicinity, and probably redactions are there. Doesn't look
    // notable but let's give some benefit of doubt.
    if (redactionsFound)
        return false; // Join + redactions or redactions + leave

    return !(joinFound && leaveFound); // Join + (maybe profile changes) + leave
}

void MessageEventModel::refreshLastUserEvents(int baseTimelineRow)
{
    if (!m_currentRoom || m_currentRoom->timelineSize() <= baseTimelineRow)
        return;

    const auto& timelineBottom = m_currentRoom->messageEvents().rbegin();
    const auto& lastSender = (*(timelineBottom + baseTimelineRow))->senderId();
    const auto limit = timelineBottom +
                std::min(baseTimelineRow + 100, m_currentRoom->timelineSize());
    for (auto it = timelineBottom + std::max(baseTimelineRow - 100, 0);
         it != limit; ++it)
    {
        if ((*it)->senderId() == lastSender)
        {
            auto idx = index(it - timelineBottom);
            emit dataChanged(idx, idx);
        }
    }
}

int MessageEventModel::rowCount(const QModelIndex& parent) const
{
    if( !m_currentRoom || parent.isValid() )
        return 0;
    return m_currentRoom->timelineSize() + m_currentRoom->pendingEvents().size();
}

inline QColor mixColors(QColor base, QColor tint, qreal mixRatio = 0.5)
{
    mixRatio = tint.alphaF() * mixRatio;
    const auto baseRatio = 1 - mixRatio;
    return QColor::fromRgbF(tint.redF() * mixRatio + base.redF() * baseRatio,
                            tint.greenF() * mixRatio + base.greenF() * baseRatio,
                            tint.blueF() * mixRatio + base.blueF() * baseRatio,
                            mixRatio + base.alphaF() * baseRatio);
}

inline QColor fadedTextColor(QColor unfadedColor, qreal fadeRatio = 0.5)
{
    return mixColors(QPalette().color(QPalette::Disabled, QPalette::Text),
                     unfadedColor, fadeRatio);
}

QColor MessageEventModel::fadedBackColor(QColor unfadedColor,
                                         qreal fadeRatio) const
{
    return mixColors(QPalette().color(QPalette::Disabled, QPalette::Base),
                     unfadedColor, fadeRatio);
}

QVariant MessageEventModel::data(const QModelIndex& idx, int role) const
{
    const auto row = idx.row();

    if (!idx.isValid() || row >= rowCount())
        return {};

    bool isPending = row < timelineBaseIndex();
    const auto timelineIt = m_currentRoom->messageEvents().crbegin() +
                                std::max(0, row - timelineBaseIndex());
    const auto pendingIt = m_currentRoom->pendingEvents().crbegin() +
                                std::min(row, timelineBaseIndex());
    const auto& evt = isPending ? **pendingIt : **timelineIt;

    using namespace Quotient;
    static Settings settings;
    if (role == Qt::DisplayRole) {
        if (evt.isRedacted()) {
            auto reason = evt.redactedBecause()->reason();
            if (reason.isEmpty())
                return tr("Redacted");

            return tr("Redacted: %1").arg(reason.toHtmlEscaped());
        }

        // clang-format off
        return visit(evt
            , [this] (const RoomMessageEvent& e) {
                // clang-format on
                using namespace MessageEventContent;

                if (e.hasTextContent() && e.mimeType().name() != "text/plain") {
                    // Naïvely assume that it's HTML
                    auto htmlBody =
                        static_cast<const TextContent*>(e.content())->body;
                    auto [cleanHtml, errorPos, errorString] =
                        HtmlFilter::fromMatrixHtml(htmlBody, m_currentRoom);
                    // If HTML is bad (or it's not HTML at all), fall back
                    // to returning the prettified plain text
                    if (errorPos != -1) {
                        cleanHtml = m_currentRoom->prettyPrint(e.plainBody());
                        // A manhole to visualise HTML errors
                        if (settings.get<bool>("Debug/html"))
                            cleanHtml +=
                                QStringLiteral("<br /><font color=\"red\">"
                                               "At pos %1: %2</font>")
                                    .arg(QString::number(errorPos), errorString);
                    }
                    return cleanHtml;
                }
                if (e.hasFileContent()) {
                    auto fileCaption =
                        e.content()->fileInfo()->originalName.toHtmlEscaped();
                    if (fileCaption.isEmpty())
                        fileCaption = m_currentRoom->prettyPrint(e.plainBody());
                    return !fileCaption.isEmpty() ? fileCaption : tr("a file");
                }
                return m_currentRoom->prettyPrint(e.plainBody());
                // clang-format off
            }
            , [this] (const RoomMemberEvent& e) {
                // clang-format on
                // FIXME: Rewind to the name that was at the time of this event
                const auto subjectName =
                    m_currentRoom->safeMemberName(e.userId()).toHtmlEscaped();
                // The below code assumes senderName output in AuthorRole
                switch( e.membership() )
                {
                    case Membership::Invite:
                    case Membership::Join: {
                        QString text {};
                        // Part 1: invites and joins
                        if (e.membership() == Membership::Invite)
                            text = tr("invited %1 to the room")
                                   .arg(subjectName);
                        else if (e.changesMembership())
                            text = tr("joined the room");

                        if (!text.isEmpty()) {
                            if (e.repeatsState())
                                text += ' '
                                    //: State event that doesn't change the state
                                    % tr("(repeated)");
                            if (!e.reason().isEmpty())
                                text += ": " + e.reason().toHtmlEscaped();
                            return text;
                        }

                        // Part 2: profile changes of joined members
                        if (e.isRename()
                            && settings.get("UI/show_rename", true)) {
                            const auto& newDisplayName =
                                e.newDisplayName().value_or(QString());
                            if (newDisplayName.isEmpty())
                                text = tr("cleared the display name");
                            else
                                text = tr("changed the display name to %1")
                                           .arg(newDisplayName.toHtmlEscaped());
                        }
                        if (e.isAvatarUpdate()
                            && settings.get("UI/show_avatar_update", true)) {
                            if (!text.isEmpty())
                                //: Joiner for member profile updates;
                                //: mind the leading and trailing spaces!
                                text += tr(" and ");
                            text += !e.newAvatarUrl()
                                            || e.newAvatarUrl()->isEmpty()
                                        ? tr("cleared the avatar")
                                        : tr("updated the avatar");
                        }
                        return text;
                    }
                    case Membership::Leave:
                        if (e.prevContent() &&
                            e.prevContent()->membership == Membership::Invite)
                        {
                            return (e.senderId() != e.userId())
                                    ? tr("withdrew %1's invitation").arg(subjectName)
                                    : tr("rejected the invitation");
                        }

                        if (e.prevContent() &&
                                e.prevContent()->membership == Membership::Ban)
                        {
                            return (e.senderId() != e.userId())
                                    ? tr("unbanned %1").arg(subjectName)
                                    : tr("self-unbanned");
                        }
                        return (e.senderId() != e.userId())
                                ? e.reason().isEmpty()
                                  ? tr("kicked %1 from the room")
                                    .arg(subjectName)
                                  : tr("kicked %1 from the room: %2")
                                    .arg(subjectName,
                                         e.reason().toHtmlEscaped())
                                : tr("left the room");
                    case Membership::Ban:
                        return (e.senderId() != e.userId())
                                ? e.reason().isEmpty()
                                  ? tr("banned %1 from the room")
                                    .arg(subjectName)
                                  : tr("banned %1 from the room: %2")
                                    .arg(subjectName,
                                         e.reason().toHtmlEscaped())
                                : tr("self-banned from the room");
                    case Membership::Knock:
                        return tr("knocked");
                    default:
                        ;
                }
                return tr("made something unknown");
                // clang-format off
            }
            , [] (const RoomCanonicalAliasEvent& e) {
                return (e.alias().isEmpty())
                        ? tr("cleared the room main alias")
                        : tr("set the room main alias to: %1").arg(e.alias());
            }
            , [] (const RoomNameEvent& e) {
                return (e.name().isEmpty())
                        ? tr("cleared the room name")
                        : tr("set the room name to: %1")
                          .arg(e.name().toHtmlEscaped());
            }
            , [this] (const RoomTopicEvent& e) {
                return (e.topic().isEmpty())
                        ? tr("cleared the topic")
                        : tr("set the topic to: %1")
                          .arg(m_currentRoom->prettyPrint(e.topic()));
            }
            , [] (const RoomAvatarEvent&) {
                return tr("changed the room avatar");
            }
            , [] (const EncryptionEvent&) {
                return tr("activated End-to-End Encryption");
            }
            , [] (const RoomCreateEvent& e) {
                return (e.isUpgrade()
                        ? tr("upgraded the room to version %1")
                        : tr("created the room, version %1")
                       ).arg(e.version().isEmpty()
                             ? "1" : e.version().toHtmlEscaped());
            }
            , [] (const RoomTombstoneEvent& e) {
                return tr("upgraded the room: %1")
                       .arg(e.serverMessage().toHtmlEscaped());
            }
            , [] (const StateEventBase& e) {
                // A small hack for state events from TWIM bot
                return e.stateKey() == "twim"
                    ? tr("updated the database", "TWIM bot updated the database")
                    : e.stateKey().isEmpty()
                    ? tr("updated %1 state", "%1 - Matrix event type")
                      .arg(e.matrixType())
                    : tr("updated %1 state for %2",
                         "%1 - Matrix event type, %2 - state key")
                      .arg(e.matrixType(), e.stateKey().toHtmlEscaped());
            }
            , tr("Unknown event")
        );
        // clang-format on
    }

    if (role == Qt::ForegroundRole) {
        using CG = QPalette::ColorGroup;
        using CR = QPalette::ColorRole;

        if (evt.isRedacted())
            return QPalette().color(CG::Disabled, CR::Text);

        if (isPending) {
            using ES = Quotient::EventStatus::Code;
            switch (pendingIt->deliveryStatus()) {
            case ES::Submitted:
            case ES::SendingFailed:
                return QPalette().color(CG::Disabled, CR::Text);
            case ES::Departed:
                return fadedTextColor(QPalette().color(CG::Active, CR::Text));
            default:;
            }
        }
        // Background highlighting mode is handled entirely in QML
        if (m_currentRoom->isEventHighlighted(&evt)
            && settings.get<QString>(QStringLiteral("UI/highlight_mode"))
                   == "text")
            return settings.get(QStringLiteral("UI/highlight_color"),
                                QStringLiteral("orange"));

        auto textColor = QPalette().color(CG::Active, CR::Text);
        if (isPending || evt.senderId() == m_currentRoom->localUser()->id())
            textColor = mixColors(textColor,
                            settings.get(QStringLiteral("UI/outgoing_color"),
                                         QStringLiteral("#4A8780")), 0.5);

        const auto* const rme = eventCast<const RoomMessageEvent>(&evt);
        return rme && rme->msgtype() != MessageEventType::Notice
                   ? textColor
                   : fadedTextColor(textColor);
    }

    if( role == Qt::ToolTipRole )
    {
        return QJsonDocument(evt.fullJson()).toJson();
    }

    if( role == EventTypeRole )
    {
        if (auto e = eventCast<const RoomMessageEvent>(&evt))
        {
            switch (e->msgtype())
            {
                case MessageEventType::Emote:
                    return "emote";
                case MessageEventType::Notice:
                    return "notice";
                case MessageEventType::Image:
                    return "image";
                default:
                    return e->hasFileContent() ? "file" : "message";
            }
        }
        if (evt.isStateEvent())
            return "state";

        return "other";
    }

    if (role == EventResolvedTypeRole)
        return EventTypeRegistry::getMatrixType(evt.type());

    if( role == AuthorRole )
    {
        // FIXME: It shouldn't be User, it should be its state "as of event"
        return QVariant::fromValue(isPending
                                   ? m_currentRoom->localUser()
                                   : m_currentRoom->user(evt.senderId()));
    }

    if (role == ContentTypeRole)
    {
        if (auto e = eventCast<const RoomMessageEvent>(&evt))
        {
            const auto& contentType = e->mimeType().name();
            return contentType == "text/plain"
                    ? QStringLiteral("text/html") : contentType;
        }
        return QStringLiteral("text/plain");
    }

    if (role == ContentRole)
    {
        if (evt.isRedacted())
        {
            const auto reason = evt.redactedBecause()->reason();
            return (reason.isEmpty())
                    ? tr("Redacted")
                    : tr("Redacted: %1").arg(reason.toHtmlEscaped());
        }

        if (auto e = eventCast<const RoomMessageEvent>(&evt))
        {
            // Cannot use e.contentJson() here because some
            // EventContent classes inject values into the copy of the
            // content JSON stored in EventContent::Base
            return e->hasFileContent()
                    ? QVariant::fromValue(e->content()->originalJson)
                    : QVariant();
        }
    }

    if( role == HighlightRole )
        return m_currentRoom->isEventHighlighted(&evt);

    if( role == SpecialMarksRole )
    {
        if (is<RedactionEvent>(evt) || is<ReactionEvent>(evt))
            return EventStatus::Hidden; // Never show, even pending

        if (isPending)
            return !settings.get<bool>("UI/suppress_local_echo")
                    ? pendingIt->deliveryStatus() : EventStatus::Hidden;

        // isReplacement?
        if (auto e = eventCast<const RoomMessageEvent>(&evt)) {
            if (!e->replacedEvent().isEmpty())
                return EventStatus::Hidden;
            if (e->isReplaced()) {
                return EventStatus::Replaced;
            }
        }

        if (is<RoomCanonicalAliasEvent>(evt)
            && !settings.get<bool>("UI/show_alias_update", true))
            return EventStatus::Hidden;

        auto* memberEvent = timelineIt->viewAs<RoomMemberEvent>();
        if (memberEvent) {
            if ((memberEvent->isJoin() || memberEvent->isLeave())
                && !settings.get<bool>("UI/show_joinleave", true))
                return EventStatus::Hidden;

            if ((memberEvent->isInvite() || memberEvent->isRejectedInvite())
                && !settings.get<bool>("UI/show_invite", true))
                return EventStatus::Hidden;

            if ((memberEvent->isBan() || memberEvent->isUnban())
                && !settings.get<bool>("UI/show_ban", true))
                return EventStatus::Hidden;

            bool hideRename =
                memberEvent->isRename()
                && (!memberEvent->isJoin() && !memberEvent->isLeave())
                && !settings.get<bool>("UI/show_rename", true);
            bool hideAvatarUpdate =
                memberEvent->isAvatarUpdate()
                && !settings.get<bool>("UI/show_avatar_update", true);
            if ((hideRename && hideAvatarUpdate)
                    || (hideRename && !memberEvent->isAvatarUpdate())
                    || (hideAvatarUpdate && !memberEvent->isRename())) {
                return EventStatus::Hidden;
            }
        }
        if (memberEvent || evt.isRedacted()) {
            if (evt.senderId() != m_currentRoom->localUser()->id()
                && evt.stateKey() != m_currentRoom->localUser()->id()
                && !settings.get<bool>("UI/show_spammy")) {
//                QElapsedTimer et; et.start();
                auto hide = !isUserActivityNotable(timelineIt);
//                qDebug() << "Checked user activity for" << evt.id() << "in" << et;
                if (hide)
                    return EventStatus::Hidden;
            }
        }

        if (evt.isRedacted())
            return settings.get<bool>("UI/show_redacted")
                    ? EventStatus::Redacted : EventStatus::Hidden;

        if (auto* stateEvt = eventCast<const StateEvent>(&evt);
            stateEvt && stateEvt->repeatsState()
            && !settings.get<bool>("UI/show_noop_events"))
            return EventStatus::Hidden;

        if (!evt.isStateEvent() && !is<RoomMessageEvent>(evt)
            && !settings.get<bool>("UI/show_unknown_events"))
            return EventStatus::Hidden;
        return EventStatus::Normal;
    }

    if( role == EventIdRole )
        return !evt.id().isEmpty() ? evt.id() : evt.transactionId();

    if( role == LongOperationRole )
    {
        if (auto e = eventCast<const RoomMessageEvent>(&evt))
            if (e->hasFileContent())
                return QVariant::fromValue(
                            m_currentRoom->fileTransferInfo(
                                isPending ? e->transactionId() : e->id()));
    }

    if( role == AnnotationRole )
        return isPending ? pendingIt->annotation() : QString();

    if( role == ReactionsRole ) {
        // Filter reactions out of all annotations and collate them by key
        struct Reaction {
            QString key;
            QStringList authorsList {};
            bool includesLocalUser = false;
        };
        std::vector<Reaction> reactions; // using vector to maintain the order
        // XXX: Should the list be ordered by the number of reactions instead?
        const auto& annotations =
            m_currentRoom->relatedEvents(evt, EventRelation::AnnotationType);
        for (const auto& a: annotations)
            if (const auto e = eventCast<const ReactionEvent>(a)) {
                auto rIt = std::find_if(reactions.begin(), reactions.end(),
                                        [&e] (const Reaction& r) {
                                            return r.key == e->key();
                                       });
                if (rIt == reactions.end())
                    rIt = reactions.insert(reactions.end(), { e->key() });

                rIt->authorsList << m_currentRoom->safeMemberName(e->senderId());
                rIt->includesLocalUser |=
                        e->senderId() == m_currentRoom->localUser()->id();
            }
        // Prepare the QML model data
        // NB: Strings are NOT HTML-escaped; QML code must take care to use
        // Text.PlainText format when displaying them
        QJsonArray qmlReactions;
        for (auto&& r: reactions) {
            const auto authorsCount = r.authorsList.size();
            if (r.authorsList.size() > 7) {
                //: When the reaction comes from too many members
                r.authorsList.replace(3, tr("%Ln more member(s)", "",
                                            r.authorsList.size() - 3));
                r.authorsList.erase(r.authorsList.begin() + 4,
                                    r.authorsList.end());
            }
            qmlReactions << QJsonObject {
                    { QStringLiteral("key"), r.key },
                    { QStringLiteral("authorsCount"), authorsCount },
                    { QStringLiteral("authors"),
                            QLocale().createSeparatedList(r.authorsList) },
                    { QStringLiteral("includesLocalUser"), r.includesLocalUser }
                };
        }
        return qmlReactions;
    }

    if( role == TimeRole || role == DateRole)
    {
        auto ts = isPending ? pendingIt->lastUpdated()
                            : makeMessageTimestamp(timelineIt);
        return role == TimeRole ? QVariant(ts) : renderDate(ts);
    }

    if (role == EventGroupingRole) {
        for (auto r = row + 1; r < rowCount(); ++r)
        {
            auto i = index(r);
            if (data(i, SpecialMarksRole) != EventStatus::Hidden)
                return data(i, DateRole) != data(idx, DateRole)
                           ? EventGrouping::ShowDateAndAuthor
                       : data(i, AuthorRole) != data(idx, AuthorRole)
                           ? EventGrouping::ShowAuthor
                           : EventGrouping::KeepPreviousGroup;
        }
        return EventGrouping::ShowDateAndAuthor; // No events before
    }

    if (role == RefRole)
        return visit(
            evt, [](const RoomCreateEvent& e) { return e.predecessor().roomId; },
            [](const RoomTombstoneEvent& e) { return e.successorRoomId(); });

    if (role == BareRichBodyRole)
    {
        auto e = eventCast<const Quotient::RoomMessageEvent>(&evt);
        if (!e || !e->hasTextContent())
            return QString();

        static const QRegularExpression quoteBlock {
            "<mx-reply>.*</mx-reply>",
            QRegularExpression::DotMatchesEverythingOption
        };
        static const QRegularExpression quoteLines("> .*(?:\n|$)");
        QString bareBody;
        if (e->mimeType().name() != "text/plain") {
            // Naïvely assume that it's HTML
            auto htmlBody =
                static_cast<const MessageEventContent::TextContent*>(e->content())->body;
            auto [cleanHtml, errorPos, errorString] =
                HtmlFilter::fromMatrixHtml(htmlBody.remove(quoteBlock), m_currentRoom);
            if (errorPos == -1) {
                bareBody = cleanHtml;
            }
        }
        if (bareBody.isEmpty()) {
            bareBody = m_currentRoom->prettyPrint(e->plainBody().remove(quoteLines));
        }
        return bareBody;
    }

    if (role == QuotationRole)
    {
        auto e = eventCast<const Quotient::RoomMessageEvent>(&evt);
        if (!e || !e->hasTextContent())
            return QString();

        static const QRegularExpression quoteLines("> .*(?:\n|$)");
        static const QRegularExpression eachLine("(.+)(?:\n|$)");
        const auto quotePrefix = QStringLiteral("> \\1\n");
        const auto authorUser = isPending
                                    ? m_currentRoom->localUser()
                                    : m_currentRoom->user(evt.senderId());
        const auto authorId = authorUser->id();

        QString quotation = e->plainBody().remove(quoteLines);
        return QStringLiteral("<%1> %2").arg(authorId, quotation).
                replace(eachLine, quotePrefix);
    }

    if (role == HtmlQuotationRole)
    {
        if (isPending)
            return QString();   // Cannot construct event link with unknown eventId
        QString quotation = data(idx, BareRichBodyRole).toString();
        if (quotation.isEmpty())
            return QString();
        const auto authorUser = m_currentRoom->user(evt.senderId());
        const QString evtLink =
            "https://matrix.to/#/" + m_currentRoom->id() + "/" + evt.id();
        const QString authorName = authorUser->displayname(m_currentRoom);
        const QString authorLink =
            Uri(authorUser->id()).toUrl(Uri::MatrixToUri).toString();

        return QStringLiteral(
            "<mx-reply><blockquote><a href=\"%1\">In reply to</a> <a href=\"%2\">%3</a><br />%4</blockquote></mx-reply>"
        ).arg(evtLink, authorLink, authorName, quotation);
    }

    return {};
}
