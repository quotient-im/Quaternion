/**************************************************************************
 *                                                                        *
 * Copyright (C) 2016 Felix Rohrbach <kde@fxrh.de>                        *
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

#include "quaternionroom.h"

#include <QtCore/QRegularExpression>

#include "lib/connection.h"

QuaternionRoom::QuaternionRoom(QMatrixClient::Connection* connection,
                               QString roomId,
                               QMatrixClient::JoinState joinState)
    : QMatrixClient::Room(connection, roomId, joinState)
{
    m_shown = false;
    m_cachedInput = "";
    connect( this, &QuaternionRoom::notificationCountChanged, this, &QuaternionRoom::countChanged );
    connect( this, &QuaternionRoom::highlightCountChanged, this, &QuaternionRoom::countChanged );
}

void QuaternionRoom::setShown(bool shown)
{
    if( shown == m_shown )
        return;
    m_shown = shown;
    if( m_shown )
    {
        resetHighlightCount();
        resetNotificationCount();
    }
}

bool QuaternionRoom::isShown()
{
    return m_shown;
}

const QuaternionRoom::Timeline& QuaternionRoom::messages() const
{
    return m_messages;
}

using QMatrixClient::TimelineItem;

void QuaternionRoom::onAddNewTimelineEvents(timeline_iter_t from)
{
    m_messages.reserve(std::distance(from, messageEvents().cend()));
    std::transform(from, messageEvents().cend(),
        std::back_inserter(m_messages),
        [=] (const TimelineItem& ti) { return Message(ti.event(), this); });
}

void QuaternionRoom::onAddHistoricalTimelineEvents(rev_iter_t from)
{
    m_messages.reserve(std::distance(from, messageEvents().crend()));
    std::transform(from, messageEvents().crend(),
        std::front_inserter(m_messages),
        [=] (const TimelineItem& ti) { return Message(ti.event(), this); });
}

void QuaternionRoom::onRedaction(const QMatrixClient::RoomEvent* before,
                                 const QMatrixClient::RoomEvent* after)
{
    Q_ASSERT(before && after);
    const auto it = std::find_if(m_messages.begin(), m_messages.end(),
        [=](const Message& m) { return m.messageEvent() == before; });
    if (it != m_messages.end())
        it->setEvent(after);
}

void QuaternionRoom::countChanged()
{
    if( m_shown )
    {
        resetNotificationCount();
        resetHighlightCount();
    }
}

const QString& QuaternionRoom::cachedInput() const
{
    return m_cachedInput;
}

void QuaternionRoom::setCachedInput(const QString& input)
{
    m_cachedInput = input;
}

/** Converts all that looks like a URL into HTML links */
void linkifyUrls(QString& htmlEscapedText)
{
    static const auto RegExpOptions =
        QRegularExpression::CaseInsensitiveOption
#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
        | QRegularExpression::OptimizeOnFirstUsageOption
#endif
        | QRegularExpression::UseUnicodePropertiesOption;

    // NOTE: htmlEscapedText is already HTML-escaped (no literal <,>,&)!

    // regexp is originally taken from Konsole (https://github.com/KDE/konsole)
    // full url:
    // protocolname:// or www. followed by anything other than whitespaces,
    // <, >, ' or ", and ends before whitespaces, <, >, ', ", ], !, ), :,
    // comma or dot
    // Note: outer parentheses are a part of C++ raw string delimiters, not of
    // the regex (see http://en.cppreference.com/w/cpp/language/string_literal).
    const QRegularExpression FullUrlRegExp(QStringLiteral(
            R"(((www\.(?!\.)|[a-z][a-z0-9+.-]*://)(&(?![lg]t;)|[^&\s<>'"])+(&(?![lg]t;)|[^&!,.\s<>'"\]):])))"
        ), RegExpOptions);
    // email address:
    // [word chars, dots or dashes]@[word chars, dots or dashes].[word chars]
    const QRegularExpression EmailAddressRegExp(QStringLiteral(
            R"((mailto:)?(\b(\w|\.|-)+@(\w|\.|-)+\.\w+\b))"
        ), RegExpOptions);

    htmlEscapedText.replace(EmailAddressRegExp,
                 QStringLiteral(R"(<a href="mailto:\2">\1\2</a>)"));
    htmlEscapedText.replace(FullUrlRegExp,
                 QStringLiteral(R"(<a href="\1">\1</a>)"));

}

QString QuaternionRoom::prettyPrint(const QString& plainText) const
{
    auto pt = QStringLiteral("<span style='white-space:pre-wrap'>") +
            plainText.toHtmlEscaped() + QStringLiteral("</span>");
    pt.replace('\n', "<br/>");

    linkifyUrls(pt);
    return pt;
}
