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

class ChatEdit;
class MessageEventModel;
class ImageProvider;

class QFrame;
class QQuickView;
class QLabel;
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
        void joinCommandEntered(const QString& roomAlias);
        void showStatusMessage(const QString& message, int timeout = 0) const;
        void readMarkerMoved();
        void readMarkerCandidateMoved();

    public slots:
        void setRoom(QuaternionRoom* room);
        void updateHeader();

        void insertMention(QString author);
        void focusInput();

        void typingChanged();
        void onMessageShownChanged(QString eventId, bool shown);
        void markShownAsRead();
        void saveFileAs(QString eventId);

    protected:
        void timerEvent(QTimerEvent* event) override;

    private slots:
        void sendInput();

    private:
        MessageEventModel* m_messageModel;
        QuaternionRoom* m_currentRoom;

        QQuickView* m_quickView;
        ImageProvider* m_imageProvider;
        ChatEdit* m_chatEdit;
        QLabel* m_currentlyTyping;
        QLabel* m_topicLabel;
        QLabel* m_roomAvatar;

        using timeline_index_t = QMatrixClient::TimelineItem::index_t;
        QVector<timeline_index_t> indicesOnScreen;
        timeline_index_t indexToMaybeRead;
        QBasicTimer maybeReadTimer;
        bool readMarkerOnScreen;
        QMap<QuaternionRoom*, QVector<QTextDocument*>> roomHistories;

        void reStartShownTimer();
        bool doSendInput();

        bool checkAndRun(const QString& args, const QString& pattern,
            std::function<void()> fn, const QString& errorMsg) const;
        bool checkAndRun1(const QString& args, const QString& pattern,
            std::function<void(QMatrixClient::Room*, QString)> fn1,
            const QString& errorMsg) const;
        bool checkAndRun2(const QString& args, const QString& pattern1,
            std::function<void(QMatrixClient::Room*, QString, QString)> fn2,
            const QString& errorMsg) const;
};
