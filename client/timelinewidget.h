#pragma once

#include "activitydetector.h"

#include <lib/eventitem.h>

#include <QtCore/QBasicTimer>

#ifndef USE_QQUICKWIDGET
#    define DISABLE_QQUICKWIDGET
#endif

#ifdef DISABLE_QQUICKWIDGET
#    include <QtQuick/QQuickView>
#else
#    include <QtQuickWidgets/QQuickWidget>
#endif

class ChatRoomWidget;
class MessageEventModel;
class ImageProvider;
class QuaternionRoom;

using TimelineBaseWidget =
#ifdef DISABLE_QQUICKWIDGET
    QQuickView;
#else
    QQuickWidget;
#endif

class TimelineWidget : public TimelineBaseWidget {
    Q_OBJECT
public:
    TimelineWidget(ChatRoomWidget* parent);
    QString selectedText() const;
    QuaternionRoom* currentRoom() const;

signals:
    void resourceRequested(const QString& idOrUri, const QString& action = {});
    void roomSettingsRequested();
    void showStatusMessage(const QString& message, int timeout = 0) const;
    void pageUpPressed();
    void pageDownPressed();
    void openExternally(int currentIndex);
    void showDetails(int currentIndex);
    void scrollViewTo(int currentIndex);
    void animateMessage(int currentIndex);

public slots:
    void setRoom(QuaternionRoom* room);
    void focusInput();
    void spotlightEvent(const QString& eventId);
    void onMessageShownChanged(const QString& eventId, bool shown);
    void markShownAsRead();
    void saveFileAs(const QString& eventId);
    void showMenu(int index, const QString& hoveredLink,
                  const QString& selectedText, bool showingDetails);
    void reactionButtonClicked(const QString& eventId, const QString& key);
    void setGlobalSelectionBuffer(const QString& text);

private:
    MessageEventModel* m_messageModel;
    QString m_selectedText;

    using timeline_index_t = Quotient::TimelineItem::index_t;
    std::vector<timeline_index_t> indicesOnScreen;
    timeline_index_t indexToMaybeRead;
    QBasicTimer maybeReadTimer;
    bool readMarkerOnScreen;
    ActivityDetector activityDetector;

    ChatRoomWidget* roomWidget() const;
    void reStartShownTimer();
    void timerEvent(QTimerEvent* qte) override;
    bool pendingMarkRead() const;
};
