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

#include "message.h"

QuaternionRoom::QuaternionRoom(QMatrixClient::Connection* connection, QString roomId)
    : QMatrixClient::Room(connection, roomId)
{
    m_shown = false;
    m_cachedInput = "";
    connect( this, &QuaternionRoom::notificationCountChanged, this, &QuaternionRoom::countChanged );
    connect( this, &QuaternionRoom::highlightCountChanged, this, &QuaternionRoom::countChanged );
}

QuaternionRoom::~QuaternionRoom()
{ }

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

inline Message* QuaternionRoom::makeMessage(QMatrixClient::Event* e)
{
    return new Message(connection(), e, this);
}

void QuaternionRoom::doAddNewMessageEvents(const QMatrixClient::Events& events)
{
    Room::doAddNewMessageEvents(events);

    m_messages.reserve(m_messages.size() + events.size());
    for (auto e: events)
        m_messages.push_back(makeMessage(e));
}

void QuaternionRoom::doAddHistoricalMessageEvents(const QMatrixClient::Events& events)
{
    Room::doAddHistoricalMessageEvents(events);

    m_messages.reserve(m_messages.size() + events.size());
    for (auto e: events)
        m_messages.push_front(makeMessage(e));
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

void QuaternionRoom::linkifyUrls(QString& text) const
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
#define FQDN "(\\w[-\\w]*\\.)+(?!\\d)\\w[-\\w]+"
    static const QRegularExpression urlDetectorMail {
        QStringLiteral("(^|\\s)(" // Criteria to match the beginning of the URL
            "\\w[^@/#\\s]*@" // authentication (the part before @)
            FQDN
        "\\b)"), // Criteria to match the end of the URL
        RegExpOptions
    };
    // Relative URLs require two dots in the series of characters. This is a bit
    // overly restrictive but eliminates dubious cases like command.com
    static const QRegularExpression urlDetectorRelative {
        QStringLiteral("(^|\\s)(" "(\\w[-\\w]*\\.)" FQDN "\\b)"), RegExpOptions
    };
    static const QRegularExpression urlDetectorAbsolute {
        QStringLiteral("(^|\\s)(" // Criteria to match the beginning of the URL
            "(?!file)([a-z][-.+\\w]+:(//)?)" // scheme
            "(\\w[^@/#\\s]*@)?" // optional authentication (the part before @)
            "(" FQDN // host name
                "|(\\d{1,3}\\.){3}\\d{1,3}" // or IPv4 address (not very strict)
                "|\\[[\\d:a-f]+\\]" // or IPv6 address (very lazy and not strict)
            ")"
            "(:\\d{1,5})?" // optional port
            "(/[^\\s]*)?" // optional query and fragment
        "\\b)"), // Criteria to match the end of the URL
        RegExpOptions
    };
#undef FQDN
    // mail regex is the least specific because of [^@\\s], run it first to
    // avoid repeated substitutions.
    text.replace(urlDetectorMail,
                 QStringLiteral("\\1<a href=\"mailto:\\2\">\\2</a>"));
    text.replace(urlDetectorRelative,
                 QStringLiteral("\\1<a href=\"http://\\2\">\\2</a>"));
    text.replace(urlDetectorAbsolute,
                 QStringLiteral("\\1<a href=\"\\2\">\\2</a>"));
}

QString QuaternionRoom::prettyPrint(const QString& plainText) const
{
    auto pt = plainText.toHtmlEscaped();
    pt.replace('\n', "<br/>");

    linkifyUrls(pt);
    return pt;
}
