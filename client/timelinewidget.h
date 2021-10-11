#pragma once

#include "activitydetector.h"

#include <eventitem.h>

#include <QtQuickWidgets/QQuickWidget>
#include <QtCore/QBasicTimer>

class ChatRoomWidget;
class MessageEventModel;
class ImageProvider;
class QuaternionRoom;

class TimelineWidget : public QQuickWidget {
    Q_OBJECT
public:
    TimelineWidget(ChatRoomWidget* chatRoomWidget);
    ~TimelineWidget() override;
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
    void viewPositionRequested(int index);
    void animateMessage(int currentIndex);

public slots:
    void setRoom(QuaternionRoom* room);
    void focusInput();
    void spotlightEvent(const QString& eventId);
    void onMessageShownChanged(int visualIndex, bool shown, bool hasReadMarker);
    void markShownAsRead();
    void saveFileAs(const QString& eventId);
    void showMenu(int index, const QString& hoveredLink,
                  const QString& selectedText, bool showingDetails);
    void reactionButtonClicked(const QString& eventId, const QString& key);
    void setGlobalSelectionBuffer(const QString& text);

private:
    MessageEventModel* m_messageModel;
    ImageProvider* m_imageProvider;
    QString m_selectedText;

    using timeline_index_t = Quotient::TimelineItem::index_t;
    std::vector<timeline_index_t> indicesOnScreen;
    timeline_index_t indexToMaybeRead;
    QBasicTimer maybeReadTimer;
    bool readMarkerOnScreen;
    ActivityDetector activityDetector;
    ChatRoomWidget* roomWidget;

    void reStartShownTimer();
    void timerEvent(QTimerEvent* qte) override;
    bool pendingMarkRead() const;
};
