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

#include <QtWidgets/QWidget>
#include <QtCore/QBasicTimer>

#include "quaternionroom.h"

#ifndef USE_QQUICKWIDGET
#define DISABLE_QQUICKWIDGET
#endif

class ChatEdit;
class MessageEventModel;
class ImageProvider;

class QFrame;
#ifdef DISABLE_QQUICKWIDGET
class QQuickView;
#else
class QQuickWidget;
#endif
class QLabel;
class QAction;
class QTextDocument;

class ChatRoomWidget: public QWidget
{
        Q_OBJECT
    public:
        explicit ChatRoomWidget(QWidget* parent = nullptr);

        void enableDebug();
        bool pendingMarkRead() const;

        QStringList findCompletionMatches(const QString& pattern) const;

    signals:
        void joinRequested(const QString& roomAlias);
        void resourceRequested(const QString& idOrUri,
                               const QString& action = {});
        void showStatusMessage(const QString& message, int timeout = 0) const;
        void readMarkerMoved();
        void readMarkerCandidateMoved();
        void pageUpPressed();
        void pageDownPressed();
        void openExternally(int currentIndex);
        void showDetails(int currentIndex);

    public slots:
        void setRoom(QuaternionRoom* room);

        void insertMention(Quotient::User* user);
        void focusInput();

        void typingChanged();
        void onMessageShownChanged(const QString& eventId, bool shown);
        void markShownAsRead();
        void saveFileAs(QString eventId);
        void quote(const QString& htmlText);
        void showMenu(int index, const QString& hoveredLink, bool showingDetails);
        void fileDrop(const QString& url);
        void textDrop(const QString& text);
        void setGlobalSelectionBuffer(QString text);
        Qt::KeyboardModifiers getModifierKeys();

    private slots:
        void sendInput();
        void encryptionChanged();
        void setHudCaption(QString newCaption);

    private:
        // Data
        MessageEventModel* m_messageModel;
        QuaternionRoom* m_currentRoom;
        ImageProvider* m_imageProvider;

#ifdef DISABLE_QQUICKWIDGET
        using timelineWidget_t = QQuickView;
#else
        using timelineWidget_t = QQuickWidget;
#endif
        // Controls
        timelineWidget_t* m_timelineWidget;
        QLabel* m_hudCaption; //< For typing and completion notifications
        QAction* m_attachAction;
        ChatEdit* m_chatEdit;

        // Supplementary/cache data members
        using timeline_index_t = Quotient::TimelineItem::index_t;
        QVector<timeline_index_t> indicesOnScreen;
        timeline_index_t indexToMaybeRead;
        QBasicTimer maybeReadTimer;
        bool readMarkerOnScreen;
        QMap<QuaternionRoom*, QVector<QTextDocument*>> roomHistories;
        QString attachedFileName;
        QString selectedText;

        void reStartShownTimer();
        QString doSendInput();

        void timerEvent(QTimerEvent* qte) override;
        void resizeEvent(QResizeEvent*) override;
        void keyPressEvent(QKeyEvent*) override;

        int maximumChatEditHeight() const;
};
