#include "timelinewidget.h"

#include "chatroomwidget.h"
#include "logging_categories.h"
#include "models/messageeventmodel.h"

#include <Quotient/events/reactionevent.h>
#include <Quotient/events/roompowerlevelsevent.h>

#include <Quotient/csapi/message_pagination.h>

#include <Quotient/networkaccessmanager.h>
#include <Quotient/settings.h>
#include <Quotient/user.h>

#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMenu>

#include <QtCore/QStringBuilder>
#include <QtGui/QClipboard>
#include <QtGui/QDesktopServices>

using Quotient::operator""_ls;

QNetworkAccessManager* TimelineWidget::NamFactory::create(QObject* parent)
{
    return new Quotient::NetworkAccessManager(parent);
}

TimelineWidget::TimelineWidget(ChatRoomWidget* chatRoomWidget)
    : QQuickWidget(chatRoomWidget)
    , m_messageModel(new MessageEventModel(this))
    , indexToMaybeRead(-1)
    , readMarkerOnScreen(false)
{
    using namespace Quotient;
    qmlRegisterUncreatableType<QuaternionRoom>(
        "Quotient", 1, 0, "Room",
        "Room objects can only be created by libQuotient");
    qmlRegisterAnonymousType<RoomMember>("Quotient", 1);
    qmlRegisterAnonymousType<GetRoomEventsJob>("Quotient", 1);
    qmlRegisterAnonymousType<MessageEventModel>("Quotient", 1);
    qmlRegisterType<Settings>("Quotient", 1, 0, "Settings");

    setResizeMode(SizeRootObjectToView);

    engine()->setNetworkAccessManagerFactory(&namFactory);

    auto* ctxt = rootContext();
    ctxt->setContextProperty("messageModel"_ls, m_messageModel);
    ctxt->setContextProperty("controller"_ls, this);

    setSource(QUrl("qrc:///qml/Timeline.qml"_ls));

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

ChatRoomWidget* TimelineWidget::roomWidget() const
{
    return static_cast<ChatRoomWidget*>(parent());
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
    indexToMaybeRead = -1;

    m_messageModel->changeRoom(newRoom);
    if (newRoom) {
        connect(newRoom, &Quotient::Room::fullyReadMarkerMoved, this, [this] {
            const auto rm = currentRoom()->fullyReadMarker();
            readMarkerOnScreen = rm != currentRoom()->historyEdge()
                                 && std::ranges::lower_bound(indicesOnScreen, rm->index())
                                        != indicesOnScreen.cend();
            reStartShownTimer();
            activityDetector.setEnabled(pendingMarkRead());
        });
        newRoom->setDisplayed(true);
    }
}

void TimelineWidget::focusInput() { roomWidget()->focusInput(); }

void TimelineWidget::spotlightEvent(const QString& eventId)
{
    auto index = m_messageModel->findRow(eventId);
    if (index >= 0) {
        emit viewPositionRequested(index);
        emit animateMessage(index);
    } else
        roomWidget()->setHudHtml("<font color=red>" % tr("Referenced message not found")
                                 % "</font>");
}

void TimelineWidget::saveFileAs(const QString& eventId)
{
    if (!currentRoom()) {
        qCWarning(TIMELINE)
            << "ChatRoomWidget::saveFileAs without an active room ignored";
        return;
    }
    const auto fileName = QFileDialog::getSaveFileName(
        this, tr("Save file as"), currentRoom()->fileNameToDownload(eventId));
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

    const auto pos = std::ranges::lower_bound(indicesOnScreen, timelineIndex);
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

    auto menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    if (currentRoom()->canRedact(eventId))
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
                    [this, modelIndex] { roomWidget()->quote(modelIndex.data().toString()); });

    auto a = menu->addAction(QIcon::fromTheme("view-list-details"),
                             tr("Show details"),
                             [this, index] { emit showDetails(index); });
    a->setCheckable(true);
    a->setChecked(showingDetails);

    const auto eventType =
        modelIndex.data(MessageEventModel::EventTypeRole).toString();
    if (eventType == "image" || eventType == "file") {
        const auto progressInfo =
            modelIndex.data(MessageEventModel::LongOperationRole).value<Quotient::FileTransferInfo>();
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
            && a->senderId() == currentRoom()->localMember().id()) //
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

void TimelineWidget::ensureLastReadEvent()
{
    auto r = currentRoom();
    if (!r)
        return;
    if (!historyRequest.isCanceled()) { // Second click cancels the request
        historyRequest.cancel();
        return;
    }
    // Store the future as is, without continuations, so that it could be cancelled
    historyRequest = r->ensureHistory(r->lastFullyReadEventId());
    historyRequest
        .then([this](auto) {
            qCDebug(TIMELINE,
                    "Loaded enough history to get the last fully read event, now scrolling");
            emit viewPositionRequested(
                m_messageModel->findRow(currentRoom()->lastFullyReadEventId()));
            emit historyRequestChanged();
        })
        .onCanceled([this] { emit historyRequestChanged(); });
}

bool TimelineWidget::isHistoryRequestRunning() const { return historyRequest.isRunning(); }

void TimelineWidget::reStartShownTimer()
{
    if (!readMarkerOnScreen || indicesOnScreen.empty()
        || indexToMaybeRead >= indicesOnScreen.back())
        return;

    static Quotient::Settings settings;
    maybeReadTimer.start(settings.get<int>("UI/maybe_read_timer", 1000), this);
    qCDebug(TIMELINE) << "Scheduled maybe-read message update:"
                      << indexToMaybeRead << "->" << indicesOnScreen.back();
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
        qCDebug(TIMELINE) << "Maybe-read message update:" << indexToMaybeRead
                          << "->" << indicesOnScreen.back();
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
