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

ChatEdit::ChatEdit(ChatRoomWidget* c) : KChatEdit(c), chatRoomWidget(c) { }

void ChatEdit::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Tab) {
        triggerCompletion();
        return;
    }

    cancelCompletion();
    KChatEdit::keyPressEvent(event);
}

void ChatEdit::startNewCompletion()
{
    const QString inputText = toPlainText();
    const int cursorPosition = textCursor().position();
    for ( m_completionInsertStart = cursorPosition; --m_completionInsertStart >= 0; )
    {
        if ( !(inputText.at(m_completionInsertStart).isLetterOrNumber() || inputText.at(m_completionInsertStart) == '@') )
            break;
    }
    ++m_completionInsertStart;
    m_completionLength = cursorPosition - m_completionInsertStart;
    m_completionList = chatRoomWidget->findCompletionMatches(inputText.mid(m_completionInsertStart, m_completionLength));
    if ( !m_completionList.isEmpty() )
    {
        m_completionCursorOffset = 0;
        m_completionListPosition = 0;
        m_completionLength = 0;
        if ( m_completionInsertStart == 0)
        {
            setText(inputText.left(m_completionInsertStart) + ": " + inputText.mid(cursorPosition));
            m_completionCursorOffset = 2;
        }
        else if ( inputText.mid(m_completionInsertStart - 2, 2) == ": ")
        {
            setText(inputText.left(m_completionInsertStart - 2) + ", : " + inputText.mid(cursorPosition));
            m_completionCursorOffset = 2;
        }
        else if ( inputText.mid(m_completionInsertStart - 1, 1) == ":")
        {
            setText(inputText.left(m_completionInsertStart - 1) + ", : " + inputText.mid(cursorPosition));
            ++m_completionInsertStart;
            m_completionCursorOffset = 2;
        }
        else
        {
            setText(inputText.left(m_completionInsertStart) + " " + inputText.mid(cursorPosition));
            m_completionCursorOffset = 1;
        }
    }
}

void ChatEdit::triggerCompletion()
{
    if (m_completionList.isEmpty())
    {
        startNewCompletion();
    }
    if (!m_completionList.isEmpty())
    {
        const QString inputText = toPlainText();
        setText( inputText.left(m_completionInsertStart)
                             + m_completionList.at(m_completionListPosition)
                             + inputText.right(inputText.length() - m_completionInsertStart - m_completionLength) );
        m_completionLength = m_completionList.at(m_completionListPosition).length();
        textCursor().setPosition( m_completionInsertStart + m_completionLength + m_completionCursorOffset );
        m_completionListPosition = (m_completionListPosition + 1) % m_completionList.length();
        emit proposedCompletion(m_completionList, m_completionListPosition);
    }
}

void ChatEdit::cancelCompletion()
{
    m_completionList.clear();
    emit cancelledCompletion();
}
