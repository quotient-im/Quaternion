#include "timelinewidget.h"

#include "chatroomwidget.h"
#include "models/messageeventmodel.h"
#include "imageprovider.h"

#include <settings.h>
#include <events/roompowerlevelsevent.h>
#include <events/reactionevent.h>
#include <csapi/message_pagination.h>
#include <user.h>

#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMenu>
#include <QtWidgets/QApplication>
#include <QtGui/QDesktopServices>
#include <QtGui/QClipboard>
#include <QtCore/QStringBuilder>

TimelineWidget::TimelineWidget(ChatRoomWidget* chatRoomWidget)
    : m_messageModel(new MessageEventModel(this))
    , m_imageProvider(new ImageProvider())
    , indexToMaybeRead(-1)
    , readMarkerOnScreen(false)
    , roomWidget(chatRoomWidget)
{
    using namespace Quotient;
    qmlRegisterUncreatableType<QuaternionRoom>(
        "Quotient", 1, 0, "Room",
        "Room objects can only be created by libQuotient");
    qmlRegisterUncreatableType<User>(
        "Quotient", 1, 0, "User",
        "User objects can only be created by libQuotient");
    qmlRegisterAnonymousType<GetRoomEventsJob>("Quotient", 1);
    qmlRegisterAnonymousType<MessageEventModel>("Quotient", 1);
    qRegisterMetaType<GetRoomEventsJob*>("GetRoomEventsJob*");
    qRegisterMetaType<User*>("User*");
    qmlRegisterType<Settings>("Quotient", 1, 0, "Settings");
    qmlRegisterUncreatableType<RoomMessageEvent>(
        "Quotient", 1, 0, "RoomMessageEvent", "RoomMessageEvent is uncreatable");

    setResizeMode(SizeRootObjectToView);

    engine()->addImageProvider(QStringLiteral("mtx"), m_imageProvider);

    auto* ctxt = rootContext();
    ctxt->setContextProperty(QStringLiteral("messageModel"), m_messageModel);
    ctxt->setContextProperty(QStringLiteral("controller"), this);

    setSource(QUrl("qrc:///qml/Timeline.qml"));

    connect(&activityDetector, &ActivityDetector::triggered, this,
            &TimelineWidget::markShownAsRead);
}

TimelineWidget::~TimelineWidget()
{
    // Clean away the view to prevent further requests to the controller
    setSource({});
}

QString TimelineWidget::selectedText() const { return m_selectedText; }

QuaternionRoom* TimelineWidget::currentRoom() const
{
    return m_messageModel->room();
}

QModelIndex TimelineWidget::indexOf(const QString& eventId) const
{
    auto row = m_messageModel->findRow(eventId);
    if (row >= 0)
        return m_messageModel->index(row);
    else
        return QModelIndex();
}

void TimelineWidget::setRoom(QuaternionRoom* newRoom)
{
    if (currentRoom() == newRoom)
        return;

    if (currentRoom()) {
        currentRoom()->setDisplayed(false);
        currentRoom()->disconnect(this);
    }
    readMarkerOnScreen = false;
    maybeReadTimer.stop();
    indicesOnScreen.clear();

    // Update the image provider upfront to allow image requests from
    // QML bindings to MessageEventModel::roomChanged
    m_imageProvider->setConnection(newRoom ? newRoom->connection() : nullptr);
    m_messageModel->changeRoom(newRoom);
    if (newRoom) {
        connect(newRoom, &Quotient::Room::readMarkerMoved, this, [this] {
            const auto rm = currentRoom()->fullyReadMarker();
            readMarkerOnScreen = rm != currentRoom()->historyEdge()
                                 && std::lower_bound(indicesOnScreen.cbegin(),
                                                     indicesOnScreen.cend(),
                                                     rm->index())
                                        != indicesOnScreen.cend();
            reStartShownTimer();
            activityDetector.setEnabled(pendingMarkRead());
        });
        newRoom->setDisplayed(true);
    }
}

void TimelineWidget::focusInput() { roomWidget->focusInput(); }

void TimelineWidget::spotlightEvent(const QString& eventId)
{
    auto index = m_messageModel->findRow(eventId);
    if (index >= 0) {
        emit viewPositionRequested(index);
        emit animateMessage(index);
    } else
        roomWidget->setHudHtml("<font color=red>"
                               % tr("Referenced message not found") % "</font>");
}

void TimelineWidget::saveFileAs(const QString& eventId)
{
    if (!currentRoom()) {
        qWarning()
            << "ChatRoomWidget::saveFileAs without an active room ignored";
        return;
    }
    // TODO: Once Qt 5.11 is dropped, use `this` instead of roomWidget
    const auto fileName =
        QFileDialog::getSaveFileName(roomWidget, tr("Save file as"),
                                     currentRoom()->fileNameToDownload(eventId));
    if (!fileName.isEmpty())
        currentRoom()->downloadFile(eventId, QUrl::fromLocalFile(fileName));
}

void TimelineWidget::onMessageShownChanged(int visualIndex, bool shown,
                                           bool hasReadMarker)
{
    const auto* room = currentRoom();
    if (!room || !room->displayed())
        return;

    // A message can be auto-marked as (fully) read if:
    // 0. The (fully) read marker is on the screen
    // 1. The message is shown on the screen now
    // 2. It's been the bottommost message on the screen for the last 1 second
    //    (or whatever UI/maybe_read_timer tells in milliseconds) and the user
    //    is active during that time
    // 3. It's below the read marker after that time

    Q_ASSERT(visualIndex <= room->timelineSize());
    const auto eventIt = room->syncEdge() - visualIndex - 1;
    const auto timelineIndex = eventIt->index();

    if (hasReadMarker) {
        readMarkerOnScreen = shown;
        if (shown) {
            indexToMaybeRead = timelineIndex;
            reStartShownTimer();
        } else
            maybeReadTimer.stop();
    }

    auto pos = std::lower_bound(indicesOnScreen.begin(), indicesOnScreen.end(),
                                timelineIndex);
    if (shown) {
        if (pos == indicesOnScreen.end() || *pos != timelineIndex) {
            indicesOnScreen.insert(pos, timelineIndex);
            if (timelineIndex == indicesOnScreen.back())
                reStartShownTimer();
        }
    } else {
        if (pos != indicesOnScreen.end() && *pos == timelineIndex)
            if (indicesOnScreen.erase(pos) == indicesOnScreen.end())
                reStartShownTimer();
    }
}

void TimelineWidget::showMenu(int index, const QString& hoveredLink,
                              const QString& selectedText, bool showingDetails)
{
    const auto modelIndex = m_messageModel->index(index, 0);
    const auto eventId =
        modelIndex.data(MessageEventModel::EventIdRole).toString();

    // TODO: Once Qt 5.11 is dropped, use `this` instead of roomWidget
    auto menu = new QMenu(roomWidget);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const auto* plEvt =
        currentRoom()->currentState().get<Quotient::RoomPowerLevelsEvent>();
    const auto localUserId = currentRoom()->localUser()->id();
    const int userPl = plEvt ? plEvt->powerLevelForUser(localUserId) : 0;
    const auto* modelUser =
        modelIndex.data(MessageEventModel::AuthorRole).value<Quotient::User*>();
    menu->addAction(QIcon::fromTheme("mail-reply-sender"),
                    tr("Reply"),
                    [this, eventId] {
                        roomWidget->reply(eventId);
                    });
    if (localUserId == modelUser->id()) {
        menu->addAction(QIcon::fromTheme("edit-entry"),
                        tr("Edit"),
                        [this, eventId] {
                            roomWidget->edit(eventId);
                        });
    }
    if (!plEvt || userPl >= plEvt->redact() || localUserId == modelUser->id())
        menu->addAction(QIcon::fromTheme("edit-delete"), tr("Redact"), this,
                        [this, eventId] { currentRoom()->redactEvent(eventId); });

    if (!selectedText.isEmpty())
        menu->addAction(tr("Copy selected text to clipboard"), this,
                        [selectedText] {
                            QApplication::clipboard()->setText(selectedText);
                        });

    if (!hoveredLink.isEmpty())
        menu->addAction(tr("Copy link to clipboard"), this, [hoveredLink] {
            QApplication::clipboard()->setText(hoveredLink);
        });

    menu->addAction(QIcon::fromTheme("link"), tr("Copy permalink to clipboard"),
                    [this, eventId] {
                        QApplication::clipboard()->setText(
                            "https://matrix.to/#/" + currentRoom()->id() + "/"
                            + QUrl::toPercentEncoding(eventId));
                    });
    menu->addAction(QIcon::fromTheme("format-text-blockquote"),
                    tr("Quote", "a verb (do quote), not a noun (a quote)"),
                    [this, modelIndex] {
                        roomWidget->quote(modelIndex.data().toString());
                    });

    auto a = menu->addAction(QIcon::fromTheme("view-list-details"),
                             tr("Show details"),
                             [this, index] { emit showDetails(index); });
    a->setCheckable(true);
    a->setChecked(showingDetails);

    const auto eventType =
        modelIndex.data(MessageEventModel::EventTypeRole).toString();
    if (eventType == "image" || eventType == "file") {
        const auto progressInfo =
            modelIndex.data(MessageEventModel::LongOperationRole)
                .value<Quotient::FileTransferInfo>();
        const bool downloaded = !progressInfo.isUpload
                                && progressInfo.completed();

        menu->addSeparator();
        menu->addAction(QIcon::fromTheme("document-open"), tr("Open externally"),
                        [this, index] { emit openExternally(index); });
        if (downloaded) {
            menu->addAction(QIcon::fromTheme("folder-open"), tr("Open Folder"),
                            [localDir = progressInfo.localDir] {
                                QDesktopServices::openUrl(localDir);
                            });
            if (eventType == "image") {
                menu->addAction(tr("Copy image to clipboard"), this,
                                [imgPath = progressInfo.localPath.path()] {
                                    QApplication::clipboard()->setImage(
                                        QImage(imgPath));
                                });
            }
        } else {
            menu->addAction(QIcon::fromTheme("edit-download"), tr("Download"),
                            [this, eventId] {
                                currentRoom()->downloadFile(eventId);
                            });
        }
        menu->addAction(QIcon::fromTheme("document-save-as"),
                        tr("Save file as..."),
                        [this, eventId] { saveFileAs(eventId); });
    }
    menu->popup(QCursor::pos());
}

void TimelineWidget::reactionButtonClicked(const QString& eventId,
                                           const QString& key)
{
    using namespace Quotient;
    const auto& annotations =
        currentRoom()->relatedEvents(eventId, EventRelation::AnnotationType);

    for (const auto& a: annotations)
        if (auto* e = eventCast<const ReactionEvent>(a);
            e != nullptr && e->key() == key
            && a->senderId() == currentRoom()->localUser()->id()) //
        {
            currentRoom()->redactEvent(a->id());
            return;
        }

    currentRoom()->postReaction(eventId, key);
}

void TimelineWidget::setGlobalSelectionBuffer(const QString& text)
{
    if (QApplication::clipboard()->supportsSelection())
        QApplication::clipboard()->setText(text, QClipboard::Selection);

    m_selectedText = text;
}

void TimelineWidget::reStartShownTimer()
{
    if (!readMarkerOnScreen || indicesOnScreen.empty()
        || indexToMaybeRead >= indicesOnScreen.back())
        return;

    static Quotient::Settings settings;
    maybeReadTimer.start(settings.get<int>("UI/maybe_read_timer", 1000), this);
    qDebug() << "Scheduled maybe-read message update:" << indexToMaybeRead
             << "->" << indicesOnScreen.back();
}

void TimelineWidget::timerEvent(QTimerEvent* qte)
{
    if (qte->timerId() != maybeReadTimer.timerId()) {
        QQuickWidget::timerEvent(qte);
        return;
    }
    maybeReadTimer.stop();
    // Only update the maybe-read message if we're tracking it
    if (readMarkerOnScreen && !indicesOnScreen.empty()
        && indexToMaybeRead < indicesOnScreen.back()) //
    {
        qDebug() << "Maybe-read message update:" << indexToMaybeRead << "->"
                 << indicesOnScreen.back();
        indexToMaybeRead = indicesOnScreen.back();
        activityDetector.setEnabled(pendingMarkRead());
    }
}

void TimelineWidget::markShownAsRead()
{
    // FIXME: a case when a single message doesn't fit on the screen.
    if (auto room = currentRoom(); room != nullptr && readMarkerOnScreen) {
        const auto iter = room->findInTimeline(indicesOnScreen.back());
        Q_ASSERT(iter != room->historyEdge());
        room->markMessagesAsRead((*iter)->id());
    }
}

bool TimelineWidget::pendingMarkRead() const
{
    if (!readMarkerOnScreen || !currentRoom())
        return false;

    const auto rm = currentRoom()->fullyReadMarker();
    return rm != currentRoom()->historyEdge() && rm->index() < indexToMaybeRead;
}

Qt::KeyboardModifiers TimelineWidget::getModifierKeys() const
{
    return QGuiApplication::keyboardModifiers();
}
