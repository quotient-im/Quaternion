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

#pragma once

#include "kchatedit.h"

class ChatRoomWidget;

class ChatEdit : public KChatEdit
{
        Q_OBJECT
    public:
        ChatEdit(ChatRoomWidget* c);

        void triggerCompletion();
        void cancelCompletion();

    signals:
        void proposedCompletion(const QStringList& allCompletions, int curIndex);
        void cancelledCompletion();

    protected:
        void keyPressEvent(QKeyEvent* event) override;

    private:
        ChatRoomWidget* chatRoomWidget;

        QStringList m_completionList;
        int m_completionListPosition;
        int m_completionInsertStart;
        int m_completionLength;
        int m_completionCursorOffset;

        void startNewCompletion();
};


