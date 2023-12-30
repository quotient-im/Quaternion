/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2015 Felix Rohrbach <kde@fxrh.de>              *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#pragma once

#include "chatedit.h"

#include <Quotient/settings.h>

#include <QtCore/QTemporaryFile>
#include <QtWidgets/QWidget>

class TimelineWidget;
class QuaternionRoom;
class MainWindow;

class QLabel;
class QAction;

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
        QLabel* m_hudCaption; //!< For typing and completion notifications
        QAction* m_attachAction;
        ChatEdit* m_chatEdit;

        QString attachedFileName;
        std::unique_ptr<QTemporaryFile> m_fileToAttach;
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
