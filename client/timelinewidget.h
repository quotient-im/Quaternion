#pragma once

#include "activitydetector.h"

#include <Quotient/eventitem.h>

#include <QtQml/QQmlNetworkAccessManagerFactory>
#include <QtQuickWidgets/QQuickWidget>

#include <QtCore/QBasicTimer>

class ChatRoomWidget;
class MessageEventModel;
class QuaternionRoom;

class TimelineWidget : public QQuickWidget {
    Q_OBJECT
public:
    TimelineWidget(ChatRoomWidget* chatRoomWidget);
    ~TimelineWidget() override;
    QString selectedText() const;
    QuaternionRoom* currentRoom() const;
    Q_INVOKABLE Qt::KeyboardModifiers getModifierKeys() const;
    Q_INVOKABLE bool isHistoryRequestRunning() const;

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
    void historyRequestChanged();

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
    void ensureLastReadEvent();

private:
    MessageEventModel* m_messageModel;
    QString m_selectedText;

    using timeline_index_t = Quotient::TimelineItem::index_t;
    std::vector<timeline_index_t> indicesOnScreen;
    timeline_index_t indexToMaybeRead;
    QBasicTimer maybeReadTimer;
    bool readMarkerOnScreen;
    ActivityDetector activityDetector;
    QFuture<void> historyRequest;

    class NamFactory : public QQmlNetworkAccessManagerFactory {
    public:
        QNetworkAccessManager* create(QObject* parent) override;
    };
    NamFactory namFactory;

    ChatRoomWidget* roomWidget() const;
    void reStartShownTimer();
    void timerEvent(QTimerEvent* qte) override;
    bool pendingMarkRead() const;
};
