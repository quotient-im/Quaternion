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

void QuaternionRoom::doAddNewMessageEvents(EventsView events)
{
    Room::doAddNewMessageEvents(events);

    m_messages.reserve(m_messages.size() + events.size());
    for (auto e: events)
        m_messages.push_back(Message(e, this));
}

void QuaternionRoom::doAddHistoricalMessageEvents(EventsView events)
{
    Room::doAddHistoricalMessageEvents(events);

    m_messages.reserve(m_messages.size() + events.size());
    for (auto e: events)
        m_messages.push_front(Message(e, this));
}

void QuaternionRoom::onRedaction(QMatrixClient::RoomEvent* before,
                                 QMatrixClient::TimelineItem& after)
{
    Room::onRedaction(before, after);
    const auto it = std::find_if(m_messages.begin(), m_messages.end(),
        [=](const Message& m) { return m.messageEvent() == before; });
    if (it != m_messages.end())
        it->setEvent(after.event());
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
// A regexp for a full-qualified domain name: at least two labels
// (including TLD), with TLD having at least two characters and
// starting with a letter (not a digit).
#define FQDN "(localhost|(\\w[-\\w]*\\.)+(?!\\d)\\w[-\\w]+)"
// https://stackoverflow.com/a/7109208
#define VALID_URI_CHARS "((&amp;)|[A-Za-z0-9%\\._~:\\/?#\\[\\]@!$'\\(\\)*+,\\;=-])*"
    static const QRegularExpression urlDetectorMail {
        QStringLiteral("(^|\\b)(mailto:)?(" // Beginning criteria of the URL
            "\\w[^@/#\\s]*@" // Authentication (the part before @)
            FQDN
        "\\b)"), // End criteria of the URL
        RegExpOptions
    };

    static const QRegularExpression urlDetectorGeneric {
        QStringLiteral("(^|&lt;|\\s|\\(|\\[)("       // Beginning criteria of the URL
            "((?!file|mailto)([a-z][-.+\\w]+:(//)?)|www\\.)" // scheme or identify http(s) with "www"
            "(\\w[^@/#\\s]*@)?" // optional authentication (the part before @)
            "(" FQDN // host name
                "|(\\d{1,3}\\.){3}\\d{1,3}" // or IPv4 address (not very strict)
                "|\\[[\\d:a-f]+\\]" // or IPv6 address (very lazy and not strict)
            ")"
            "(:\\d{1,5})?" // optional port
            "(\\/" VALID_URI_CHARS ")?" // optional query and fragment
        ")(?=(&gt;|\\W?( |$)))"), // End criteria of the URL
        RegExpOptions
    };
#undef FQDN
#undef VALID_URI
    // mail regex is less specific because of [^@/#\\s]* part; run it first to avoid
    // repeated substitutions.
    htmlEscapedText.replace(urlDetectorMail,
                 QStringLiteral("\\1<a href=\"mailto:\\3\">\\2\\3</a>"));
    htmlEscapedText.replace(urlDetectorGeneric,
                 QStringLiteral("\\1<a href=\"\\2\">\\2</a>"));
}

QString QuaternionRoom::prettyPrint(const QString& plainText) const
{
    auto pt = QStringLiteral("<span style='white-space:pre-wrap'>") +
            plainText.toHtmlEscaped() + QStringLiteral("</span>");
    pt.replace('\n', "<br/>");

    linkifyUrls(pt);
    return pt;
}
