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

#include "chatroomwidget.h"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QAction>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>
#include <QtGui/QClipboard>
#include <QtGui/QDesktopServices>

#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#ifdef DISABLE_QQUICKWIDGET
#include <QtQuick/QQuickView>
#else
#include <QtQuickWidgets/QQuickWidget>
#endif
#include <QtCore/QRegularExpression>
#include <QtCore/QStringBuilder>
#include <QtCore/QLocale>

#include <events/roommessageevent.h>
#include <csapi/message_pagination.h>
#include <user.h>
#include <connection.h>
#include <settings.h>
#include "models/messageeventmodel.h"
#include "imageprovider.h"
#include "chatedit.h"

static const auto DefaultPlaceholderText =
        ChatRoomWidget::tr("Choose a room to send messages or enter a command...");

ChatRoomWidget::ChatRoomWidget(QWidget* parent)
    : QWidget(parent)
    , m_messageModel(new MessageEventModel(this))
    , m_currentRoom(nullptr)
    , indexToMaybeRead(-1)
    , readMarkerOnScreen(false)
{
    {
        using namespace Quotient;
        qmlRegisterUncreatableType<QuaternionRoom>("Quotient", 1, 0, "Room",
            "Room objects can only be created by libQuotient");
        qmlRegisterUncreatableType<User>("Quotient", 1, 0, "User",
            "User objects can only be created by libQuotient");
        qmlRegisterType<GetRoomEventsJob>();
        qRegisterMetaType<GetRoomEventsJob*>("GetRoomEventsJob*");
        qRegisterMetaType<User*>("User*");
        qmlRegisterType<Settings>("Quotient", 1, 0, "Settings");
        qmlRegisterUncreatableType<RoomMessageEvent>("Quotient", 1, 0,
            "RoomMessageEvent", "RoomMessageEvent is uncreatable");
    }

    m_timelineWidget = new timelineWidget_t;
    qDebug() << "Rendering QML with"
             << timelineWidget_t::staticMetaObject.className();
    auto* qmlContainer =
#ifdef DISABLE_QQUICKWIDGET
            QWidget::createWindowContainer(m_timelineWidget, this);
#else
            m_timelineWidget;
#endif // Use different objects but the same method with the same parameters
    qmlContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_timelineWidget->setResizeMode(timelineWidget_t::SizeRootObjectToView);

    m_imageProvider = new ImageProvider();
    m_timelineWidget->engine()
            ->addImageProvider(QStringLiteral("mtx"), m_imageProvider);

    auto* ctxt = m_timelineWidget->rootContext();
    ctxt->setContextProperty(QStringLiteral("messageModel"), m_messageModel);
    ctxt->setContextProperty(QStringLiteral("controller"), this);
    ctxt->setContextProperty(QStringLiteral("debug"), QVariant(false));
    ctxt->setContextProperty(QStringLiteral("room"), nullptr);

    m_timelineWidget->setSource(QUrl("qrc:///qml/Timeline.qml"));

    m_hudCaption = new QLabel();
    m_hudCaption->setWordWrap(true);

    auto attachButton = new QToolButton();
    attachButton->setAutoRaise(true);
    m_attachAction = new QAction(QIcon::fromTheme("mail-attachment"),
                                 tr("Attach"), attachButton);
    m_attachAction->setCheckable(true);
    m_attachAction->setDisabled(true);
    connect(m_attachAction, &QAction::triggered, this, [this] (bool checked) {
        if (checked)
            attachedFileName =
                    QFileDialog::getOpenFileName(this, tr("Attach file"));
        else
            attachedFileName.clear();

        if (!attachedFileName.isEmpty())
        {
            m_chatEdit->setPlaceholderText(
                tr("Add a message to the file or just push Enter"));
            emit showStatusMessage(tr("Attaching %1").arg(attachedFileName));
        } else {
            m_attachAction->setChecked(false);
            m_chatEdit->setPlaceholderText(DefaultPlaceholderText);
            emit showStatusMessage(tr("Attaching cancelled"), 3000);
        }
    });
    attachButton->setDefaultAction(m_attachAction);

    m_chatEdit = new ChatEdit(this);
    m_chatEdit->setPlaceholderText(DefaultPlaceholderText);
    m_chatEdit->setAcceptRichText(false);
    m_chatEdit->setMaximumHeight(maximumChatEditHeight());
    connect( m_chatEdit, &KChatEdit::returnPressed, this, &ChatRoomWidget::sendInput );
    connect(m_chatEdit, &KChatEdit::copyRequested, this, [=] {
        QApplication::clipboard()->setText(
            m_chatEdit->textCursor().hasSelection() ? m_chatEdit->textCursor().selectedText() : selectedText
        );
    });
    connect(m_chatEdit, &ChatEdit::proposedCompletion, this,
            [=](const QStringList& matches, int pos)
            {
                setHudCaption(
                    tr("Next completion: %1")
                    .arg(QStringList(matches.mid(pos, 5)).join(", ")) );
            });
    connect(m_chatEdit, &ChatEdit::cancelledCompletion,
            this, &ChatRoomWidget::typingChanged);

    {
        Quotient::Settings s;
        QString styleSheet;
        const auto fontFamily = s.value("UI/Fonts/timeline_family").toString();
        if (!fontFamily.isEmpty())
            styleSheet += "font-family: " + fontFamily + ";";
        const auto fontPointSize = s.value("UI/Fonts/timeline_pointSize");
        if (fontPointSize.toReal() > 0.0)
            styleSheet += "font-size: " + fontPointSize.toString() + "pt;";
        if (!styleSheet.isEmpty())
            setStyleSheet(styleSheet);
    }

    auto* layout = new QVBoxLayout();
    layout->addWidget(qmlContainer);
    layout->addWidget(m_hudCaption);
    {
        auto inputLayout = new QHBoxLayout;
        inputLayout->addWidget(attachButton);
        inputLayout->addWidget(m_chatEdit);
        layout->addLayout(inputLayout);
    }
    setLayout(layout);
}

void ChatRoomWidget::enableDebug()
{
    QQmlContext* ctxt = m_timelineWidget->rootContext();
    ctxt->setContextProperty(QStringLiteral("debug"), true);
}

void ChatRoomWidget::setRoom(QuaternionRoom* room)
{
    if (m_currentRoom == room) {
        m_chatEdit->setFocus();
        return;
    }

    if( m_currentRoom )
    {
        m_currentRoom->setDisplayed(false);
        m_currentRoom->setCachedInput( m_chatEdit->toPlainText() );
        roomHistories.insert(m_currentRoom, m_chatEdit->history());
        m_currentRoom->connection()->disconnect(this);
        m_currentRoom->disconnect( this );
    }
    readMarkerOnScreen = false;
    maybeReadTimer.stop();
    indicesOnScreen.clear();
    attachedFileName.clear();
    m_attachAction->setChecked(false);
    m_chatEdit->cancelCompletion();

    m_currentRoom = room;
    m_attachAction->setEnabled(m_currentRoom != nullptr);
    if( m_currentRoom )
    {
        using namespace Quotient;
        m_imageProvider->setConnection(room->connection());
        m_chatEdit->setText( m_currentRoom->cachedInput() );
        m_chatEdit->setHistory(roomHistories.value(m_currentRoom));
        m_chatEdit->setFocus();
        m_chatEdit->moveCursor(QTextCursor::End);
        connect( m_currentRoom, &Room::typingChanged,
                 this, &ChatRoomWidget::typingChanged );
        connect( m_currentRoom, &Room::readMarkerMoved, this, [this] {
            const auto rm = m_currentRoom->readMarker();
            readMarkerOnScreen =
                rm != m_currentRoom->timelineEdge() &&
                std::lower_bound( indicesOnScreen.begin(), indicesOnScreen.end(),
                                 rm->index() ) != indicesOnScreen.end();
            reStartShownTimer();
            emit readMarkerMoved();
        });
        connect( m_currentRoom, &Room::encryption,
                 this, &ChatRoomWidget::encryptionChanged);
        connect(m_currentRoom->connection(), &Connection::loggedOut,
                this, [this]
        {
            qWarning() << "Logged out, escaping the room";
            setRoom(nullptr);
        });
        m_currentRoom->setDisplayed(true);
    } else
        m_imageProvider->setConnection(nullptr);
    m_timelineWidget->rootContext()
            ->setContextProperty(QStringLiteral("room"), room);
    typingChanged();
    encryptionChanged();

    m_messageModel->changeRoom( m_currentRoom );
}

void ChatRoomWidget::typingChanged()
{
    if (!m_currentRoom || m_currentRoom->usersTyping().isEmpty())
    {
        m_hudCaption->clear();
        return;
    }
    QStringList typingNames;
    for(auto user: m_currentRoom->usersTyping())
    {
        typingNames << m_currentRoom->roomMembername(user);
    }
    setHudCaption( tr("Currently typing: %1")
                   .arg(typingNames.join(QStringLiteral(", "))) );
}

void ChatRoomWidget::encryptionChanged()
{
    m_chatEdit->setReadOnly(m_currentRoom && m_currentRoom->usesEncryption());
    m_chatEdit->setPlaceholderText(
        m_currentRoom
            ? m_currentRoom->usesEncryption()
                ? tr("Sending encrypted messages is not supported yet")
                : tr("Send a message (over %1) or enter a command...",
                     "%1 is the protocol used by the server (usually HTTPS)")
                  .arg(m_currentRoom->connection()->homeserver()
                       .scheme().toUpper())
            : DefaultPlaceholderText);
}

void ChatRoomWidget::setHudCaption(QString newCaption)
{
    m_hudCaption->setText("<i>" + newCaption + "</i>");
}

void ChatRoomWidget::insertMention(Quotient::User* user)
{
    m_chatEdit->insertMention(user->displayname(m_currentRoom));
}

void ChatRoomWidget::focusInput()
{
    m_chatEdit->setFocus();
}

/**
 * \brief Split the string into the specified number of parts
 * The function takes \p s and splits it into \p maxParts parts using \p sep
 * for the separator. Empty parts are skipped. If there are more than
 * \p maxParts parts in the string, the last returned part includes
 * the remainder of the string; if there are fewer parts, the missing parts
 * are filled with empty strings.
 * \return the vector of references to the original string, one reference for
 * each part.
 */
QVector<QString> lazySplitRef(const QString& s, QChar sep, int maxParts)
{
    QVector<QString> parts { maxParts };
    int pos = 0, nextPos = 0;
    for (; maxParts > 1 && (nextPos = s.indexOf(sep, pos)) > -1; --maxParts)
    {
        parts[parts.size() - maxParts] = s.mid(pos, nextPos - pos);
        while (s[++nextPos] == sep)
            ;
        pos = nextPos;
    }
    parts[parts.size() - maxParts] = s.mid(pos);
    return parts;
}

QString ChatRoomWidget::doSendInput()
{
    auto text = m_chatEdit->toPlainText();
    if (!attachedFileName.isEmpty())
    {
        Q_ASSERT(m_currentRoom != nullptr);
        auto txnId = m_currentRoom->postFile(text.isEmpty() ?
                            QUrl(attachedFileName).fileName() : text,
                        QUrl::fromLocalFile(attachedFileName));

        attachedFileName.clear();
        m_attachAction->setChecked(false);
        m_chatEdit->setPlaceholderText(DefaultPlaceholderText);
        return {};
    }

    if ( text.isEmpty() )
        return tr("There's nothing to send");

    static const auto ReFlags = QRegularExpression::DotMatchesEverythingOption;

    static const QRegularExpression
            CommandRe { QStringLiteral("^/([^ ]+)( +(.*))?\\s*$"), ReFlags },
            RoomIdRE { QStringLiteral("^(#[-0-9a-z._=]+)|(!\\S+):\\S+$"), ReFlags },
            UserIdRE { QStringLiteral("^@[-0-9a-zA-Z._=/]+:\\S+$"), ReFlags },
            HtmlTagRE { QStringLiteral("<[^>]+>"), ReFlags };

    // Process a command
    const auto matches = CommandRe.match(text);
    const auto command = matches.capturedRef(1);
    const auto argString = matches.captured(3);

    // Commands available without a current room
    if (command == "join")
    {
        if (!argString.contains(RoomIdRE))
            return tr("/join argument doesn't look like a room ID or alias");
        emit joinRequested(argString);
        return {};
    }
    if (command == "quit")
    {
        qApp->closeAllWindows();
        return {};
    }
    // --- Add more roomless commands here
    if (!m_currentRoom)
    {
        if (text.startsWith('/'))
            return tr("There's no such /command outside of room.");

        return tr("You should select a room to send messages.");
    }

    if (!text.startsWith('/'))
    {
        m_currentRoom->postPlainText(text);
        return {};
    }
    if (text[1] == '/')
    {
        text.remove(0, 1);
        m_currentRoom->postPlainText(text);
        return {};
    }

    // Commands available only in the room context
    using namespace Quotient;
    if (command == "leave" || command == "part")
    {
        if (!argString.isEmpty())
            return tr("Sending a farewell message is not supported yet."
                      " If you intended to leave another room, switch to it"
                      " and type /leave there.");

        m_currentRoom->leaveRoom();
        return {};
    }
    if (command == "forget")
    {
        if (argString.isEmpty())
            return tr("/forget must be followed by the room id/alias,"
                      " even for the current room");
        if (!argString.contains(RoomIdRE))
            return tr("%1 doesn't look like a room id or alias").arg(argString);

        // Forget the specified room using the current room's connection
        m_currentRoom->connection()->forgetRoom(argString);
        return {};
    }
    if (command == "invite")
    {
        if (argString.isEmpty())
            return tr("/invite <memberId>");
        if (!argString.contains(UserIdRE))
            return tr("%1 doesn't look like a user ID").arg(argString);

        m_currentRoom->inviteToRoom(argString);
        return {};
    }
    if (command == "kick" || command == "ban")
    {
        const auto args = lazySplitRef(argString, ' ', 2);
        if (args.front().isEmpty())
            return tr("/%1 <userId> <reason>").arg(command.toString());
        if (!UserIdRE.match(args.front()).hasMatch())
            return tr("%1 doesn't look like a user id")
                    .arg(args.front());

        if (command == "ban")
            m_currentRoom->ban(args.front(), args.back());
        else {
            auto* user = m_currentRoom->user(args.front());
            if (m_currentRoom->memberJoinState(user) != JoinState::Join)
                return tr("%1 is not a member of this room")
                        .arg(user->fullName(m_currentRoom));
            m_currentRoom->kickMember(user->id(), args.back());
        }
        return {};
    }
    if (command == "unban")
    {
        if (argString.isEmpty())
            return tr("/unban <userId>");
        if (!argString.contains(UserIdRE))
            return tr("/unban argument doesn't look like a user ID");

        m_currentRoom->unban(argString);
        return {};
    }
    if (command == "ignore" || command == "unignore")
    {
        if (argString.isEmpty())
            return tr("/ignore <userId>");
        if (!argString.contains(UserIdRE))
            return tr("/ignore argument doesn't look like a user ID");

        if (auto* user = m_currentRoom->user(argString))
        {
            if (command == "ignore")
                user->ignore();
            else
                user->unmarkIgnore();
            return {};
        }
        return tr("Couldn't find user %1 on the server").arg(argString);
    }
    using MsgType = RoomMessageEvent::MsgType;
    if (command == "me")
    {
        if (argString.isEmpty())
            return tr("/me needs an argument");
        m_currentRoom->postMessage(argString, MsgType::Emote);
        return {};
    }
    if (command == "notice")
    {
        if (argString.isEmpty())
            return tr("/notice needs an argument");
        m_currentRoom->postMessage(argString, MsgType::Notice);
        return {};
    }
    if (command == "shrug") // Peeked at Discord
    {
        m_currentRoom->postPlainText((argString.isEmpty() ? "" : argString + " ") +
                                     "¯\\_(ツ)_/¯");
        return {};
    }
    if (command == "topic")
    {
        m_currentRoom->setTopic(argString);
        return {};
    }
    if (command == "nick")
    {
        m_currentRoom->localUser()->rename(argString);
        return {};
    }
    if (command == "roomnick")
    {
        m_currentRoom->localUser()->rename(argString, m_currentRoom);
        return {};
    }
    if (command == "pm" || command == "msg")
    {
        const auto args = lazySplitRef(argString, ' ', 2);
        if (args.front().isEmpty() || (args.back().isEmpty() && command == "msg"))
            return tr("/%1 <memberId> <message>").arg(command.toString());
        if (RoomIdRE.match(args.front()).hasMatch() && command == "msg")
        {
            if (auto* room = m_currentRoom->connection()->room(args.front()))
            {
                room->postPlainText(args.back());
                return {};
            }
            return tr("%1 doesn't seem to have joined room %2")
                    .arg(m_currentRoom->localUser()->id(), args.front());
        }
        if (UserIdRE.match(args.front()).hasMatch())
        {
            if (args.back().isEmpty())
                m_currentRoom->connection()->requestDirectChat(args.front());
            else
                m_currentRoom->connection()->doInDirectChat(args.front(),
                    [msg=args.back()] (Room* dc) { dc->postPlainText(msg); });
            return {};
        }

        return tr("%1 doesn't look like a user id or room alias")
                .arg(args.front());
    }
    if (command == "html")
    {
        // Very crude HTML-to-plaintext conversion - just strip all the tags
        auto plainText = argString;
        plainText.replace(HtmlTagRE, QString());
        m_currentRoom->postHtmlText(plainText, argString);
        return {};
    }
    if (command == "query" || command == "dc")
    {
        if (argString.isEmpty())
            return tr("/%1 <memberId>").arg(command.toString());
        if (!argString.contains(UserIdRE))
            return tr("%1 doesn't look like a user id").arg(argString);

        m_currentRoom->connection()->requestDirectChat(argString);
        return {};
    }
    // --- Add more room commands here
    qDebug() << "Unknown command:" << command;
    return tr("Unknown /command. Use // to send this line literally");
}

void ChatRoomWidget::sendInput()
{
    auto result = doSendInput();
    if (result.isEmpty())
        m_chatEdit->saveInput();
    else
        emit showStatusMessage(result, 5000);
}

QStringList ChatRoomWidget::findCompletionMatches(const QString& pattern) const
{
    QStringList matches;
    if (m_currentRoom)
    {
        for(auto name: m_currentRoom->memberNames() )
        {
            if ( name.startsWith(pattern, Qt::CaseInsensitive) )
            {
                int ircSuffixPos = name.indexOf(" (IRC)");
                if ( ircSuffixPos != -1 )
                    name.truncate(ircSuffixPos);
                matches.append(name);
            }
        }
        std::sort(matches.begin(), matches.end(),
            [] (const QString& s1, const QString& s2)
                { return s1.localeAwareCompare(s2) < 0; });
        matches.removeDuplicates();
    }
    return matches;
}

void ChatRoomWidget::saveFileAs(QString eventId)
{
    if (!m_currentRoom)
    {
        qWarning()
            << "ChatRoomWidget::saveFileAs without an active room ignored";
        return;
    }
    auto fileName = QFileDialog::getSaveFileName(
                this, tr("Save file as"),
                m_currentRoom->fileNameToDownload(eventId));
    if (!fileName.isEmpty())
        m_currentRoom->downloadFile(eventId, QUrl::fromLocalFile(fileName));
}

void ChatRoomWidget::onMessageShownChanged(const QString& eventId, bool shown)
{
    if (!m_currentRoom || !m_currentRoom->displayed())
        return;

    // A message can be auto-marked as read (as soon as the user is active), if:
    // 0. The read marker exists and is on the screen
    // 1. The message is shown on the screen now
    // 2. It's been the bottommost message on the screen for the last 1 second
    // 3. It's below the read marker

    const auto readMarker = m_currentRoom->readMarker();
    if (readMarker != m_currentRoom->timelineEdge() &&
            readMarker->event()->id() == eventId)
    {
        readMarkerOnScreen = shown;
        if (shown)
        {
            qDebug() << "Read marker is on-screen, at" << *readMarker;
            indexToMaybeRead = readMarker->index();
            reStartShownTimer();
        } else
        {
            qDebug() << "Read marker is off-screen";
            qDebug() << "Bottommost shown message index was" << indexToMaybeRead;
            maybeReadTimer.stop();
        }
    }

    const auto iter = m_currentRoom->findInTimeline(eventId);
    if (iter == m_currentRoom->timelineEdge())
    {
        qWarning() << "Event" << eventId
                   << "is not in the timeline (local echo?)";
        return;
    }
    const auto timelineIndex = iter->index();
    auto pos = std::lower_bound(indicesOnScreen.begin(), indicesOnScreen.end(),
                                timelineIndex);
    if (shown)
    {
        if (pos == indicesOnScreen.end() || *pos != timelineIndex)
        {
            indicesOnScreen.insert(pos, timelineIndex);
            if (timelineIndex == indicesOnScreen.back())
                reStartShownTimer();
        }
    } else
    {
        if (pos != indicesOnScreen.end() && *pos == timelineIndex)
            if (indicesOnScreen.erase(pos) == indicesOnScreen.end())
                reStartShownTimer();
    }
}

void ChatRoomWidget::quote(const QString& htmlText)
{
    Quotient::SettingsGroup sg { QStringLiteral("UI") };
    const auto type = sg.get<int>("quote_type");
    const auto defaultStyle = QStringLiteral("> \\1\n");
    const auto defaultRegex = QStringLiteral("(.+)(?:\n|$)");
    auto style = sg.get<QString>("quote_style");
    auto regex = sg.get<QString>("quote_regex");

    if (style.isEmpty())
        style = defaultStyle;
    if (regex.isEmpty())
        regex = defaultRegex;

    QTextDocument document;
    document.setHtml(htmlText);
    QString sendString;

    switch (type)
    {
        case 0:
            sendString = document.toPlainText()
                .replace(QRegularExpression(defaultRegex), defaultStyle);
            break;
        case 1:
            sendString = document.toPlainText()
                .replace(QRegularExpression(regex), style);
            break;
        case 2:
            sendString = QLocale().quoteString(document.toPlainText()) + "\n";
            break;
    }

    m_chatEdit->insertPlainText(sendString);
}

void ChatRoomWidget::showMenu(int index, const QString& hoveredLink,
                              bool showingDetails)
{
    const auto modelIndex = m_messageModel->index(index, 0);
    const auto eventId = modelIndex.data(MessageEventModel::EventIdRole).toString();

    QMenu menu;
    menu.addAction(QIcon::fromTheme("edit-delete"), tr("Redact"), [=] {
        m_currentRoom->redactEvent(eventId);
    });
    if (!hoveredLink.isEmpty())
    {
        menu.addAction(tr("Copy link to clipboard"), [=] {
            QApplication::clipboard()->setText(hoveredLink);
        });
    }
    menu.addAction(QIcon::fromTheme("link"), tr("Copy permalink to clipboard"), [=] {
        QApplication::clipboard()->setText("https://matrix.to/#/" +
            m_currentRoom->id() + "/" + QUrl::toPercentEncoding(eventId));
    });
    menu.addAction(QIcon::fromTheme("format-text-blockquote"),
                   tr("Quote", "a verb (do quote), not a noun (a quote)"), [=] {
        emit quote(modelIndex.data().toString());
    });
    auto a = menu.addAction(QIcon::fromTheme("view-list-details"), tr("Show details"), [=] {
        emit showDetails(index);
    });
    a->setCheckable(true);
    a->setChecked(showingDetails);

    const auto eventType = modelIndex.data(MessageEventModel::EventTypeRole).toString();
    if (eventType == "image" || eventType == "file")
    {
        const auto progressInfo = modelIndex.data(MessageEventModel::SpecialMarksRole)
            .value<Quotient::FileTransferInfo>();
        const bool downloaded = !progressInfo.isUpload && progressInfo.completed();

        menu.addSeparator();
        menu.addAction(QIcon::fromTheme("document-open"), tr("Open externally"), [=] {
            emit openExternally(index);
        });
        menu.addAction(QIcon::fromTheme("folder-open"), tr("Open Folder"), [=] {
            if (!downloaded)
                m_currentRoom->downloadFile(eventId);

            QDesktopServices::openUrl(progressInfo.localDir);
        });
        if (!downloaded)
        {
            menu.addAction(QIcon::fromTheme("edit-download"), tr("Download"), [=] {
                m_currentRoom->downloadFile(eventId);
            });
        }
        menu.addAction(QIcon::fromTheme("document-save-as"), tr("Save file as..."), [=] {
            saveFileAs(eventId);
        });
    }
    menu.exec(QCursor::pos());
}

void ChatRoomWidget::setGlobalSelectionBuffer(QString text)
{
    if (QApplication::clipboard()->supportsSelection())
        QApplication::clipboard()->setText(text, QClipboard::Selection);

    selectedText = text;
}

void ChatRoomWidget::reStartShownTimer()
{
    if (!readMarkerOnScreen || indicesOnScreen.empty() ||
            indexToMaybeRead >= indicesOnScreen.back())
        return;

    maybeReadTimer.start(1000, this);
    qDebug() << "Scheduled maybe-read message update:"
             << indexToMaybeRead << "->" << indicesOnScreen.back();
}

void ChatRoomWidget::timerEvent(QTimerEvent* qte)
{
    if (qte->timerId() != maybeReadTimer.timerId())
    {
        QWidget::timerEvent(qte);
        return;
    }
    maybeReadTimer.stop();
    // Only update the maybe-read message if we're tracking it
    if (readMarkerOnScreen && !indicesOnScreen.empty() &&
            indexToMaybeRead < indicesOnScreen.back())
    {
        qDebug() << "Maybe-read message update:" << indexToMaybeRead
                 << "->" << indicesOnScreen.back();
        indexToMaybeRead = indicesOnScreen.back();
        emit readMarkerCandidateMoved();
    }
}

void ChatRoomWidget::resizeEvent(QResizeEvent*)
{
    m_chatEdit->setMaximumHeight(maximumChatEditHeight());
}

void ChatRoomWidget::keyPressEvent(QKeyEvent* event)
{
    // This only handles keypresses not handled by ChatEdit; in particular,
    // this means that PageUp/PageDown below are actually Ctrl-PageUp/PageDown
    switch(event->key()) {
        case Qt::Key_PageUp:
            emit pageUpPressed();
            break;
        case Qt::Key_PageDown:
            emit pageDownPressed();
            break;
    }
}

int ChatRoomWidget::maximumChatEditHeight() const
{
    return maximumHeight() / 3;
}

void ChatRoomWidget::markShownAsRead()
{
    // FIXME: a case when a single message doesn't fit on the screen.
    if (m_currentRoom && readMarkerOnScreen)
    {
        const auto iter = m_currentRoom->findInTimeline(indicesOnScreen.back());
        Q_ASSERT( iter != m_currentRoom->timelineEdge() );
        m_currentRoom->markMessagesAsRead((*iter)->id());
    }
}

bool ChatRoomWidget::pendingMarkRead() const
{
    if (!readMarkerOnScreen || !m_currentRoom)
        return false;

    const auto rm = m_currentRoom->readMarker();
    return rm != m_currentRoom->timelineEdge() && rm->index() < indexToMaybeRead;
}

void ChatRoomWidget::fileDrop(const QString& url)
{
    attachedFileName = QUrl(url).path();
    m_attachAction->setChecked(true);
    m_chatEdit->setPlaceholderText(
        tr("Add a message to the file or just push Enter"));
    emit showStatusMessage(tr("Attaching %1").arg(attachedFileName));
}

void ChatRoomWidget::textDrop(const QString& text)
{
    m_chatEdit->setText(text);
}

Qt::KeyboardModifiers ChatRoomWidget::getModifierKeys()
{
    return QGuiApplication::keyboardModifiers();
}
