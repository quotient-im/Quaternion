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

#include <QtGui/QTextCursor>

class ChatRoomWidget;

class ChatEdit : public KChatEdit
{
        Q_OBJECT
    public:
        using completions_t = QVector<QPair<QString, QUrl>>;

        ChatEdit(ChatRoomWidget* c);

        void triggerCompletion();
        void cancelCompletion();
        bool isCompletionActive();

        void insertMention(QString author, QUrl url);

    public slots:
        void switchContext(QObject* contextKey) override;

    signals:
        void proposedCompletion(const QStringList& allCompletions, int curIndex);
        void cancelledCompletion();
        void insertFromMimeDataRequested(const QMimeData* source);

    protected:
        bool canInsertFromMimeData(const QMimeData* source) const override;
        void insertFromMimeData(const QMimeData* source) override;

    private:
        ChatRoomWidget* chatRoomWidget;

        QTextCursor completionCursor;
        /// Text/href pairs for completion
        completions_t completionMatches;
        int matchesListPosition;

        bool pickingMentions = false;

        /// \brief Initialise a new completion
        ///
        /// \return true if completion matches exist for the current entry;
        ///         false otherwise
        bool initCompletion();
        void appendMentionAt(QTextCursor& cursor, QString mention,
                             QUrl mentionUrl, bool select);
        void keyPressEvent(QKeyEvent* event) override;
};


