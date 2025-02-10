/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2015 Felix Rohrbach <kde@fxrh.de>              *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#pragma once

#include "chatedit.h"
#include "htmlfilter.h"

#include <Quotient/settings.h>

#include <QtCore/QTemporaryFile>
#include <QtWidgets/QWidget>

namespace Quotient {
class Connection;
}

class TimelineWidget;
class QuaternionRoom;
class MainWindow;

class QLabel;
class QAction;

class ChatRoomWidget : public QWidget
{
        Q_OBJECT
    public:
        explicit ChatRoomWidget(MainWindow* parent = nullptr);
        TimelineWidget* timelineWidget() const;
        QuaternionRoom* currentRoom() const;

        // Helpers for m_chatEdit

        ChatEdit::completions_t findCompletionMatches(const QString& pattern) const;
        QString matrixHtmlFromMime(const QMimeData* data) const;
        void checkDndEvent(QDropEvent* event) const;

    public slots:
        void setRoom(QuaternionRoom* newRoom);
        void insertMention(const QString &userId);
        void attachImage(const QImage& img, const QList<QUrl>& sources);
        QString attachFile(const QString& localPath);
        void dropFile(const QString& localPath);
        QString checkAttachment();
        void cancelAttaching();
        void focusInput();

        //! Set a line above the message input, with optional list of member displaynames
        void setHudHtml(const QString& htmlCaption,
                        const QStringList& plainTextNames = {});

        void showStatusMessage(const QString& message, int timeout = 0) const;
        void showCompletions(QStringList matches, int pos);

        void typingChanged();
        void quote(const QString& htmlText);

    private slots:
        void sendInput();
        void encryptionChanged();

    private:
        TimelineWidget* m_timelineWidget;
        QLabel* m_hudCaption; //!< For typing and completion notifications
        QAction* m_attachAction;
        ChatEdit* m_chatEdit;

        std::unique_ptr<QFile> m_fileToAttach;
        Quotient::SettingsGroup m_uiSettings;

        MainWindow* mainWindow() const;
        Quotient::Connection* currentConnection() const;

        QString sendFile();
        void sendMessage();
        void sendSelection(int fromPosition, HtmlFilter::Options htmlFilterOptions);
        [[nodiscard]] QString sendCommand(QStringView command,
                                          const QString& argString);

        void resizeEvent(QResizeEvent*) override;
        void keyPressEvent(QKeyEvent* event) override;
        void dragEnterEvent(QDragEnterEvent* event) override;
        void dropEvent(QDropEvent* event) override;

        int maximumChatEditHeight() const;
};
