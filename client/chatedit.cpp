/**************************************************************************
 *                                                                        *
 * Copyright (C) 2017 Kitsune Ral <kitsune-ral@users.sf.net>
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

#include "chatedit.h"

#include <QtGui/QKeyEvent>

#include "chatroomwidget.h"

ChatEdit::ChatEdit(ChatRoomWidget* c)
    : KChatEdit(c), chatRoomWidget(c), matchesListPosition(0)
{ }

void ChatEdit::keyPressEvent(QKeyEvent* event)
{
    pickingMentions = false;
    if (event->key() == Qt::Key_Tab) {
        triggerCompletion();
        return;
    }

    cancelCompletion();
    KChatEdit::keyPressEvent(event);
}

void ChatEdit::switchContext(QObject* contextKey)
{
    cancelCompletion();
    KChatEdit::switchContext(contextKey);
}

QString ChatEdit::sanitizeMention(QString mentionText)
{
    if (mentionText.startsWith('/'))
        mentionText.push_front('/');
    return mentionText;
}

void ChatEdit::appendTextAtCursor(const QString& text, bool select)
{
    completionCursor.insertText(text);
    completionCursor.movePosition(QTextCursor::PreviousCharacter,
        select ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor, text.size());
}

void ChatEdit::startNewCompletion()
{
    completionCursor = textCursor();
    completionCursor.clearSelection();
    while ( completionCursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor) )
    {
        QChar firstChar = completionCursor.selectedText().at(0);
        if (!firstChar.isLetterOrNumber() && firstChar != '@')
        {
            completionCursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
            break;
        }
    }
    completionMatches =
        chatRoomWidget->findCompletionMatches(completionCursor.selectedText());
    if ( !completionMatches.isEmpty() )
    {
        matchesListPosition = 0;
        auto lookBehindCursor = completionCursor;
        if ( lookBehindCursor.atStart() )
        {
            appendTextAtCursor(QStringLiteral(": "), false);
            return;
        }
        for (auto stringBefore: {QLatin1String(":"), QLatin1String(": ")})
        {
            lookBehindCursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
            if ( lookBehindCursor.selectedText().startsWith(stringBefore) )
            {
                lookBehindCursor.insertText(QStringLiteral(", "));
                appendTextAtCursor(QStringLiteral(": "), false);
                return;
            }
        }
        appendTextAtCursor(QStringLiteral(" "), false);
    }
}

void ChatEdit::triggerCompletion()
{
    if (completionMatches.isEmpty())
        startNewCompletion();

    if (!completionMatches.isEmpty())
    {
        appendTextAtCursor(
            sanitizeMention(completionMatches.at(matchesListPosition)), true);
        ensureCursorVisible(); // The real one, not completionCursor
        auto completionHL = completionCursor.charFormat();
        completionHL.setUnderlineStyle(QTextCharFormat::DashUnderline);
        setExtraSelections({ { completionCursor, completionHL } });
        emit proposedCompletion(completionMatches, matchesListPosition);
        matchesListPosition = (matchesListPosition + 1) % completionMatches.length();
    }
}

void ChatEdit::cancelCompletion()
{
    completionMatches.clear();
    setExtraSelections({});
    emit cancelledCompletion();
}

void ChatEdit::insertMention(QString author)
{
    // The order of inserting text below is such to be convenient for the user
    // to undo in case the primitive intelligence below fails.
    auto cursor = textCursor();
    author = sanitizeMention(author);
    insertPlainText(author);
    cursor.movePosition(QTextCursor::PreviousCharacter,
                        QTextCursor::MoveAnchor, author.size());

    // Add spaces and a colon around the inserted string if necessary.
    if (cursor.position() > 0 &&
            document()->characterAt(cursor.position() - 1).isLetterOrNumber())
        cursor.insertText(QStringLiteral(" "));

    while (cursor.movePosition(QTextCursor::PreviousCharacter) &&
           document()->characterAt(cursor.position()).isSpace());
    QString postfix;
    if (cursor.atStart())
        postfix = QStringLiteral(":");
    if (pickingMentions && document()->characterAt(cursor.position()) == ':')
    {
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        cursor.insertText(QStringLiteral(","));
        postfix = QStringLiteral(":");
    }
    auto currentChar = document()->characterAt(textCursor().position());
    if (textCursor().atBlockEnd() ||
            currentChar.isLetterOrNumber() || currentChar == '.')
        postfix.push_back(' ');
    if (!postfix.isEmpty())
        insertPlainText(postfix);
    pickingMentions = true;
}
