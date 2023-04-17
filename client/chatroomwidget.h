/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2015 Felix Rohrbach <kde@fxrh.de>              *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#pragma once

#include "chatedit.h"

#include <settings.h>

#include <QtWidgets/QWidget>
#include <QtWidgets/QToolButton>
#include <QtCore/QModelIndex>

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
        enum Modes {
            Default,
            Replying,
            Editing,
        };
        enum TextFormat {
            Unspecified,
            Markdown,
            Plaintext,
            Html,
        };

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
        void edit(const QString& eventId);
        void reply(const QString& eventId);
        void fileDrop(const QString& url);
        void htmlDrop(const QString& html);
        void textDrop(const QString& text);

    private slots:
        void sendInput();
        void encryptionChanged();

    private:
        TimelineWidget* m_timelineWidget;
        QLabel* m_hudCaption; //< For typing and completion notifications
        QToolButton* m_modeIndicator;
        QAction* m_attachAction;
        ChatEdit* m_chatEdit;

        int mode;
        QString referencedEventId;

        QString attachedFileName;
        QTemporaryFile* m_fileToAttach;
        Quotient::SettingsGroup m_uiSettings;

        MainWindow* mainWindow() const;
        QuaternionRoom* currentRoom() const;

        void setDefaultMode();
        bool setReferringMode(const int newMode, const QString& eventId,
                              const char* icon_name);
        QModelIndex referencedEventIndex();

        void sendFile();
        void sendMessage();
        [[nodiscard]] QString sendCommand(QStringView command,
                                          const QString& argString);
        void sendMessageFromFragment(const QTextDocumentFragment& text,
                                     enum TextFormat textFormat = Unspecified);

        void resizeEvent(QResizeEvent*) override;
        void keyPressEvent(QKeyEvent* event) override;

        int maximumChatEditHeight() const;
};
