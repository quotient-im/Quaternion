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

#pragma once

#include "chatedit.h"

#include <settings.h>

#include <QtWidgets/QWidget>

class TimelineWidget;
class QuaternionRoom;
class MainWindow;

class QLabel;
class QAction;
class QTextDocument;
class QMimeData;
class QTemporaryFile;

namespace Quotient {
class User;
}

class ChatRoomWidget : public QWidget
{
        Q_OBJECT
    public:
        using completions_t = ChatEdit::completions_t;

        explicit ChatRoomWidget(MainWindow* parent = nullptr);
        TimelineWidget* timelineWidget() const;

        completions_t findCompletionMatches(const QString& pattern) const;

    public slots:
        void setRoom(QuaternionRoom* newRoom);
        void insertMention(Quotient::User* user);
        void focusInput();

        /// Set a line just above the message input, with optional list of
        /// member displaynames
        void setHudHtml(const QString& htmlCaption,
                        const QStringList& plainTextNames = {});

        void typingChanged();
        void quote(const QString& htmlText);
        void fileDrop(const QString& url);
        void htmlDrop(const QString& html);
        void textDrop(const QString& text);

    private slots:
        void sendInput();
        void encryptionChanged();

    private:
        TimelineWidget* m_timelineWidget;
        QLabel* m_hudCaption; //< For typing and completion notifications
        QAction* m_attachAction;
        ChatEdit* m_chatEdit;

        QString attachedFileName;
        QTemporaryFile* m_fileToAttach;
        Quotient::SettingsGroup m_uiSettings;

        MainWindow* mainWindow() const;
        QuaternionRoom* currentRoom() const;

        void sendFile();
        void sendMessage();
        [[nodiscard]] QString sendCommand(QStringView command,
                                          const QString& argString);

        void resizeEvent(QResizeEvent*) override;
        void keyPressEvent(QKeyEvent* event) override;

        int maximumChatEditHeight() const;
};
