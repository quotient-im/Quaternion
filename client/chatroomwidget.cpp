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
#include <QtGui/QTextCursor> // for last-minute message fixups before sending
#include <QtGui/QTextDocumentFragment> // to produce plain text from /html
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
#include <QtCore/QTemporaryFile>
#include <QtCore/QMimeData>

#include <events/roommessageevent.h>
#include <events/roompowerlevelsevent.h>
#include <events/reactionevent.h>
#include <csapi/message_pagination.h>
#include <user.h>
#include <connection.h>
#include <uri.h>
#include <settings.h>
#include "models/messageeventmodel.h"
#include "imageprovider.h"
#include "chatedit.h"
#include "htmlfilter.h"

static const auto DefaultPlaceholderText =
        ChatRoomWidget::tr("Choose a room to send messages or enter a command...");

static constexpr auto MaxNamesToShow = 5;
static constexpr auto SampleSizeForHud = 3;
Q_STATIC_ASSERT(MaxNamesToShow > SampleSizeForHud);

ChatRoomWidget::ChatRoomWidget(QWidget* parent)
    : QWidget(parent)
    , m_messageModel(new MessageEventModel(this))
    , m_uiSettings("UI")
    , indexToMaybeRead(-1)
    , readMarkerOnScreen(false)
{
    {
        using namespace Quotient;
        qmlRegisterUncreatableType<QuaternionRoom>("Quotient", 1, 0, "Room",
            "Room objects can only be created by libQuotient");
        qmlRegisterUncreatableType<User>("Quotient", 1, 0, "User",
            "User objects can only be created by libQuotient");
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
        qmlRegisterAnonymousType<GetRoomEventsJob>("Quotient", 1);
        qmlRegisterAnonymousType<MessageEventModel>("Quotient", 1);
#else
        qmlRegisterType<GetRoomEventsJob>();
        qmlRegisterType<MessageEventModel>();
#endif
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

    m_timelineWidget->setSource(QUrl("qrc:///qml/Timeline.qml"));

    {
        m_hudCaption = new QLabel();
        m_hudCaption->setWordWrap(true);
        auto f = m_hudCaption->font();
        f.setItalic(true);
        m_hudCaption->setFont(f);
        m_hudCaption->setTextFormat(Qt::RichText);
    }

    auto attachButton = new QToolButton();
    attachButton->setAutoRaise(true);
    m_attachAction = new QAction(QIcon::fromTheme("mail-attachment"),
                                 tr("Attach"), attachButton);
    m_attachAction->setCheckable(true);
    m_attachAction->setDisabled(true);
    connect(m_attachAction, &QAction::triggered, this, [this] (bool checked) {
        if (checked)
        {
            attachedFileName =
                    QFileDialog::getOpenFileName(this, tr("Attach file"));
        } else {
            if (m_fileToAttach->isOpen())
                m_fileToAttach->remove();
            attachedFileName.clear();
        }

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

    m_fileToAttach = new QTemporaryFile(this);

    m_chatEdit = new ChatEdit(this);
    m_chatEdit->setPlaceholderText(DefaultPlaceholderText);
    m_chatEdit->setAcceptRichText(true); // m_uiSettings.get("rich_text_editor", false);
    m_chatEdit->setMaximumHeight(maximumChatEditHeight());
    connect(m_chatEdit, &KChatEdit::returnPressed, this,
            &ChatRoomWidget::sendInput);
    connect(m_chatEdit, &KChatEdit::copyRequested, this, [=] {
        QApplication::clipboard()->setText(
            m_chatEdit->textCursor().hasSelection()
                ? m_chatEdit->textCursor().selectedText()
                : selectedText);
    });
    connect(m_chatEdit, &ChatEdit::insertImageRequested, this,
            [=](const QImage& image) {
                if (currentRoom() == nullptr || m_fileToAttach->isOpen())
                    return;

                m_fileToAttach->open();
                image.save(m_fileToAttach, "PNG");

                attachedFileName = m_fileToAttach->fileName();
                m_attachAction->setChecked(true);
                m_chatEdit->setPlaceholderText(
                    tr("Add a message to the file or just push Enter"));
                emit showStatusMessage(tr("Attaching an image from clipboard"));
            });
    connect(m_chatEdit, &ChatEdit::proposedCompletion, this,
            [this](QStringList matches, int pos) {
                Q_ASSERT(pos >= 0 && pos < matches.size());
                // If the completion list is MaxNamesToShow or shorter, show all
                // of it; if it's longer, show SampleSizeForHud entries and
                // append how many more matches are there.
                // #344: in any case, drop the current match from the list
                // ("Next completion:" showing the current match looks wrong)

                switch (matches.size()) {
                case 0:
                    setHudHtml(tr("No completions"));
                    return;
                case 1:
                    setHudHtml({}); // That one match is already in the text
                    return;
                default:;
                }
                matches.removeAt(pos); // Drop the current match (#344)

                // Replenish the tail of the list from the beginning, if needed
                std::rotate(matches.begin(), matches.begin() + pos,
                            matches.end());
                if (matches.size() > MaxNamesToShow) {
                    const auto moreIt = matches.begin() + SampleSizeForHud;
                    *moreIt = tr("%Ln more completions", "",
                                 matches.size() - SampleSizeForHud);
                    matches.erase(moreIt + 1, matches.end());
                }
                setHudHtml(tr("Next completion:"), matches);
            });
    // When completion is cancelled, show typing users, if any
    connect(m_chatEdit, &ChatEdit::cancelledCompletion,
            this, &ChatRoomWidget::typingChanged);

    {
        QString styleSheet;
        const auto& fontFamily =
            m_uiSettings.get<QString>("Fonts/timeline_family");
        if (!fontFamily.isEmpty())
            styleSheet += "font-family: " + fontFamily + ";";
        const auto& fontPointSize =
            m_uiSettings.value("Fonts/timeline_pointSize");
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

QuaternionRoom* ChatRoomWidget::currentRoom() const { return m_messageModel->room(); }

void ChatRoomWidget::setRoom(QuaternionRoom* newRoom)
{
    if (currentRoom() == newRoom) {
        focusInput();
        return;
    }

    if (currentRoom()) {
        currentRoom()->connection()->disconnect(this);
        currentRoom()->disconnect(this);
    }
    readMarkerOnScreen = false;
    maybeReadTimer.stop();
    indicesOnScreen.clear();
    attachedFileName.clear();
    m_attachAction->setChecked(false);
    if (m_fileToAttach->isOpen())
        m_fileToAttach->remove();

    m_messageModel->changeRoom(newRoom);
    m_attachAction->setEnabled(newRoom != nullptr);
    m_chatEdit->switchContext(newRoom);
    if (newRoom) {
        using namespace Quotient;
        m_imageProvider->setConnection(newRoom->connection());
        focusInput();
        connect(newRoom, &Room::typingChanged, //
                this, &ChatRoomWidget::typingChanged);
        connect(newRoom, &Room::readMarkerMoved, this, [this] {
            const auto rm = currentRoom()->readMarker();
            readMarkerOnScreen = rm != currentRoom()->timelineEdge()
                                 && std::lower_bound(indicesOnScreen.cbegin(),
                                                     indicesOnScreen.cend(),
                                                     rm->index())
                                        != indicesOnScreen.cend();
            reStartShownTimer();
            emit readMarkerMoved();
        });
        connect(newRoom, &Room::encryption, //
                this, &ChatRoomWidget::encryptionChanged);
        connect(newRoom->connection(), &Connection::loggedOut, this, [this] {
            qWarning() << "Logged out, escaping the room";
            setRoom(nullptr);
        });
    } else
        m_imageProvider->setConnection(nullptr);
    typingChanged();
    encryptionChanged();
}

void ChatRoomWidget::spotlightEvent(QString eventId)
{
    auto index = m_messageModel->findRow(eventId);
    if (index >= 0) {
        emit scrollViewTo(index);
        emit animateMessage(index);
    } else
        setHudHtml("<font color=red>" % tr("Referenced message not found")
                   % "</font>");
}

void ChatRoomWidget::typingChanged()
{
    if (!currentRoom() || currentRoom()->usersTyping().isEmpty())
    {
        setHudHtml({});
        return;
    }
    const auto& usersTyping = currentRoom()->usersTyping();
    QStringList typingNames;
    typingNames.reserve(MaxNamesToShow);
    const auto endIt = usersTyping.size() > MaxNamesToShow
                       ? usersTyping.cbegin() + SampleSizeForHud
                       : usersTyping.cend();
    for (auto it = usersTyping.cbegin(); it != endIt; ++it)
        typingNames << currentRoom()->safeMemberName((*it)->id());

    if (usersTyping.size() > MaxNamesToShow) {
        typingNames.push_back(
            //: The number of users in the typing or completion list
            tr("%L1 more").arg(usersTyping.size() - SampleSizeForHud));
    }
    setHudHtml(tr("Currently typing:"), typingNames);
}

void ChatRoomWidget::encryptionChanged()
{
    m_chatEdit->setPlaceholderText(
        currentRoom()
            ? currentRoom()->usesEncryption()
                ? tr("Send a message (no end-to-end encryption support yet)...")
                : tr("Send a message (over %1) or enter a command...",
                     "%1 is the protocol used by the server (usually HTTPS)")
                  .arg(currentRoom()->connection()->homeserver()
                       .scheme().toUpper())
            : DefaultPlaceholderText);
}

void ChatRoomWidget::setHudHtml(const QString& htmlCaption,
                                const QStringList& plainTextNames)
{
    if (htmlCaption.isEmpty()) { // Fast track
        m_hudCaption->clear();
        return;
    }

    auto hudText = htmlCaption;

    if (!plainTextNames.empty()) {
        QStringList namesToShow;
        namesToShow.reserve(plainTextNames.size());

        // Elide names that don't fit the HUD line width
        // NB: averageCharWidth() accounts for a list separator appended by
        // QLocale::createSeparatedList() appends. It would be ideal
        // to subtract the specific separator width but there's no way to get
        // the list separator from QLocale()
        // (https://bugreports.qt.io/browse/QTBUG-48510)
        const auto& fm = m_hudCaption->fontMetrics();
        for (const auto& name: plainTextNames) {
            auto elided =
                fm.elidedText(name, Qt::ElideMiddle,
                              m_hudCaption->width() - fm.averageCharWidth());
            // Make sure an elided name takes a new line
            namesToShow.push_back(
                (elided != name ? "<br/>" : "") + elided.toHtmlEscaped());
        }

        hudText += ' ' + namesToShow.join(", ");
    }
    m_hudCaption->setText(hudText);
}

void ChatRoomWidget::insertMention(Quotient::User* user)
{
    Q_ASSERT(currentRoom() != nullptr);
    m_chatEdit->insertMention(
        user->displayname(currentRoom()),
        Quotient::Uri(user->id()).toUrl(Quotient::Uri::MatrixToUri));
    m_chatEdit->setFocus();
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

void ChatRoomWidget::sendFile()
{
    Q_ASSERT(currentRoom() != nullptr);
    const auto& description = m_chatEdit->toPlainText();
    auto txnId = currentRoom()->postFile(description.isEmpty()
                                             ? QUrl(attachedFileName).fileName()
                                             : description,
                                         QUrl::fromLocalFile(attachedFileName));

    if (m_fileToAttach->isOpen())
        m_fileToAttach->remove();
    attachedFileName.clear();
    m_attachAction->setChecked(false);
    m_chatEdit->setPlaceholderText(DefaultPlaceholderText);
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
void sendMarkdown(QuaternionRoom* room, const QTextDocumentFragment& text)
{
    room->postHtmlText(text.toPlainText(),
                       HtmlFilter::toMatrixHtml(text.toHtml(), room,
                                                HtmlFilter::ConvertMarkdown));
}
#endif

void ChatRoomWidget::sendMessage()
{
    if (m_chatEdit->toPlainText().startsWith("//"))
        QTextCursor(m_chatEdit->document()).deleteChar();

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    if (m_uiSettings.get("auto_markdown", false)) {
        sendMarkdown(currentRoom(),
                     QTextDocumentFragment(m_chatEdit->document()));
        return;
    }
#endif
    const auto& plainText = m_chatEdit->toPlainText();
    const auto& htmlText =
        HtmlFilter::toMatrixHtml(m_chatEdit->toHtml(), currentRoom());
    Q_ASSERT(!plainText.isEmpty() && !htmlText.isEmpty());
    // Send plain text if htmlText has no markup or just <br/> elements
    // (those are easily represented as line breaks in plain text)
    static const QRegularExpression MarkupRE { "<(?![Bb][Rr])" };
    if (htmlText.contains(MarkupRE))
        currentRoom()->postHtmlText(plainText, htmlText);
    else
        currentRoom()->postPlainText(plainText);
}

static const auto NothingToSendMsg =
    ChatRoomWidget::tr("There's nothing to send");

QString ChatRoomWidget::sendCommand(const QStringRef& command,
                                    const QString& argString)
{
    static const auto ReFlags = QRegularExpression::DotMatchesEverythingOption
                                | QRegularExpression::DontCaptureOption;

    // FIXME: copy-paste from lib/util.cpp
    static const auto ServerPartPattern =
        QStringLiteral("(\\[[^]]+\\]|[-[:alnum:].]+)" // Either IPv6 address or
                                                      // hostname/IPv4 address
                       "(:\\d{1,5})?" // Optional port
        );
    static const auto UserIdPattern =
        QString("@[-[:alnum:]._=/]+:" % ServerPartPattern);

    static const QRegularExpression
        RoomIdRE { "^([#!][^:[:space:]]+):" % ServerPartPattern % '$', ReFlags },
        UserIdRE { '^' % UserIdPattern % '$', ReFlags };
    Q_ASSERT(RoomIdRE.isValid() && UserIdRE.isValid());

    // Commands available without a current room
    if (command == "join")
    {
        if (!argString.contains(RoomIdRE))
            return tr("/join argument doesn't look like a room ID or alias");
        emit resourceRequested(argString, "join");
        return {};
    }
    if (command == "quit")
    {
        qApp->closeAllWindows();
        return {};
    }
    // --- Add more roomless commands here
    if (!currentRoom())
    {
        return tr("There's no such /command outside of room.");
    }

    // Commands available only in the room context
    using namespace Quotient;
    if (command == "leave" || command == "part")
    {
        if (!argString.isEmpty())
            return tr("Sending a farewell message is not supported yet."
                      " If you intended to leave another room, switch to it"
                      " and type /leave there.");

        currentRoom()->leaveRoom();
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
        currentRoom()->connection()->forgetRoom(argString);
        return {};
    }
    if (command == "invite")
    {
        if (argString.isEmpty())
            return tr("/invite <memberId>");
        if (!argString.contains(UserIdRE))
            return tr("%1 doesn't look like a user ID").arg(argString);

        currentRoom()->inviteToRoom(argString);
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
            currentRoom()->ban(args.front(), args.back());
        else {
            auto* const user = currentRoom()->user(args.front());
            if (currentRoom()->memberJoinState(user) != JoinState::Join)
                return tr("%1 is not a member of this room")
                    .arg(user ? user->fullName(currentRoom()) : args.front());
            currentRoom()->kickMember(user->id(), args.back());
        }
        return {};
    }
    if (command == "unban")
    {
        if (argString.isEmpty())
            return tr("/unban <userId>");
        if (!argString.contains(UserIdRE))
            return tr("/unban argument doesn't look like a user ID");

        currentRoom()->unban(argString);
        return {};
    }
    if (command == "ignore" || command == "unignore")
    {
        if (argString.isEmpty())
            return tr("/ignore <userId>");
        if (!argString.contains(UserIdRE))
            return tr("/ignore argument doesn't look like a user ID");

        if (auto* user = currentRoom()->user(argString))
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
        currentRoom()->postMessage(argString, MsgType::Emote);
        return {};
    }
    if (command == "notice")
    {
        if (argString.isEmpty())
            return tr("/notice needs an argument");
        currentRoom()->postMessage(argString, MsgType::Notice);
        return {};
    }
    if (command == "shrug") // Peeked at Discord
    {
        currentRoom()->postPlainText((argString.isEmpty() ? "" : argString + " ") +
                                     "¯\\_(ツ)_/¯");
        return {};
    }
    if (command == "roomname")
    {
        currentRoom()->setName(argString);
        return {};
    }
    if (command == "topic")
    {
        currentRoom()->setTopic(argString);
        return {};
    }
    if (command == "nick" || command == "mynick")
    {
        currentRoom()->localUser()->rename(argString);
        return {};
    }
    if (command == "roomnick" || command == "myroomnick")
    {
        currentRoom()->localUser()->rename(argString, currentRoom());
        return {};
    }
    if (command == "pm" || command == "msg")
    {
        const auto args = lazySplitRef(argString, ' ', 2);
        if (args.front().isEmpty() || (args.back().isEmpty() && command == "msg"))
            return tr("/%1 <memberId> <message>").arg(command.toString());
        if (RoomIdRE.match(args.front()).hasMatch() && command == "msg")
        {
            if (auto* room = currentRoom()->connection()->room(args.front()))
            {
                room->postPlainText(args.back());
                return {};
            }
            return tr("%1 doesn't seem to have joined room %2")
                   .arg(currentRoom()->localUser()->id(), args.front());
        }
        if (UserIdRE.match(args.front()).hasMatch())
        {
            if (args.back().isEmpty())
                currentRoom()->connection()->requestDirectChat(args.front());
            else
                currentRoom()->connection()->doInDirectChat(args.front(),
                    [msg=args.back()] (Room* dc) { dc->postPlainText(msg); });
            return {};
        }

        return tr("%1 doesn't look like a user id or room alias")
                .arg(args.front());
    }
    if (command == "plain") {
        // argString eats away leading spaces, so can't be used here
        static const auto CmdLen = QStringLiteral("/plain ").size();
        const auto& plainMsg = m_chatEdit->toPlainText().mid(CmdLen);
        if (plainMsg.isEmpty())
            return NothingToSendMsg;
        currentRoom()->postPlainText(plainMsg);
        return {};
    }
    if (command == "html")
    {
        // Assuming Matrix HTML, convert it to Qt and load to a fragment in
        // order to produce a plain text version (maybe introduce
        // filterMatrixHtmlToPlainText() one day instead...); then convert
        // back to Matrix HTML to produce the (clean) rich text version
        // of the message
        const auto& [cleanQtHtml, errorPos, errorString] =
            HtmlFilter::fromMatrixHtml(argString, currentRoom(),
                                       HtmlFilter::Validate);
        if (errorPos != -1)
            return tr("At pos %1: %2",
                      "%1 is a position of the error; %2 is the error message")
                   .arg(errorPos).arg(errorString);

        const auto& fragment = QTextDocumentFragment::fromHtml(cleanQtHtml);
        currentRoom()->postHtmlText(fragment.toPlainText(),
                                    HtmlFilter::toMatrixHtml(fragment.toHtml(),
                                                             currentRoom()));
        return {};
    }
    if (command == "md") {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        // Select everything after /md and one whitespace character after it
        // (leading whitespaces have meaning in Markdown)
        QTextCursor c(m_chatEdit->document());
        c.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 4);
        c.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        sendMarkdown(currentRoom(), c.selection());
        return {};
#else
        return tr("Your build of Quaternion doesn't support Markdown");
#endif
    }
    if (command == "query" || command == "dc")
    {
        if (argString.isEmpty())
            return tr("/%1 <memberId>").arg(command.toString());
        if (!argString.contains(UserIdRE))
            return tr("%1 doesn't look like a user id").arg(argString);

        currentRoom()->connection()->requestDirectChat(argString);
        return {};
    }
    // --- Add more room commands here
    qDebug() << "Unknown command:" << command;
    return tr("Unknown /command. Use // to send this line literally");
}

void ChatRoomWidget::sendInput()
{
    if (!attachedFileName.isEmpty())
        sendFile();
    else {
        const auto& text = m_chatEdit->toPlainText();
        QString error;
        if (text.isEmpty())
            error = NothingToSendMsg;
        else if (text.startsWith('/') && !text.midRef(1).startsWith('/')) {
            QRegularExpression cmdSplit(
                    "(\\w+)(?:\\s+(.*))?",
                    QRegularExpression::DotMatchesEverythingOption);
            const auto& blanksMatch = cmdSplit.match(text, 1);
            error = sendCommand(blanksMatch.capturedRef(1),
                                blanksMatch.captured(2));
        } else if (!currentRoom())
            error = tr("You should select a room to send messages.");
        else
            sendMessage();
        if (!error.isEmpty()) {
            emit showStatusMessage(error, 5000);
            return;
        }
    }

    m_chatEdit->saveInput();
}

ChatRoomWidget::completions_t
ChatRoomWidget::findCompletionMatches(const QString& pattern) const
{
    completions_t matches;
    if (currentRoom()) {
        const auto& users = currentRoom()->users();
        for (auto user: users) {
            using Quotient::Uri;
            if (user->displayname(currentRoom())
                    .startsWith(pattern, Qt::CaseInsensitive)
                || user->id().startsWith(pattern, Qt::CaseInsensitive))
                matches.push_back({ user->displayname(currentRoom()),
                                    Uri(user->id()).toUrl(Uri::MatrixToUri) });
        }
        std::sort(matches.begin(), matches.end(),
            [] (const auto& p1, const auto& p2)
                { return p1.first.localeAwareCompare(p2.first) < 0; });
    }
    return matches;
}

void ChatRoomWidget::saveFileAs(QString eventId)
{
    if (!currentRoom()) {
        qWarning()
            << "ChatRoomWidget::saveFileAs without an active room ignored";
        return;
    }
    const auto fileName =
        QFileDialog::getSaveFileName(this, tr("Save file as"),
                                     currentRoom()->fileNameToDownload(eventId));
    if (!fileName.isEmpty())
        currentRoom()->downloadFile(eventId, QUrl::fromLocalFile(fileName));
}

void ChatRoomWidget::onMessageShownChanged(const QString& eventId, bool shown)
{
    const auto* room = currentRoom();
    if (!room || !room->displayed())
        return;

    // A message can be auto-marked as read (as soon as the user is active), if:
    // 0. The read marker exists and is on the screen
    // 1. The message is shown on the screen now
    // 2. It's been the bottommost message on the screen for the last 1 second
    //    (or whatever UI/maybe_read_timer tells in milliseconds)
    // 3. It's below the read marker

    if (const auto readMarker = room->readMarker();
        readMarker != room->timelineEdge()
        && readMarker->event()->id() == eventId) //
    {
        readMarkerOnScreen = shown;
        if (shown) {
            qDebug() << "Read marker is on-screen, at" << *readMarker;
            indexToMaybeRead = readMarker->index();
            reStartShownTimer();
        } else {
            qDebug() << "Read marker is off-screen";
            qDebug() << "Bottommost shown message index was" << indexToMaybeRead;
            maybeReadTimer.stop();
        }
    }

    const auto iter = room->findInTimeline(eventId);
    if (iter == room->timelineEdge()) {
        qWarning() << "Event" << eventId
                   << "is not in the timeline (local echo?)";
        return;
    }
    const auto timelineIndex = iter->index();
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

void ChatRoomWidget::quote(const QString& htmlText)
{
    const auto type = m_uiSettings.get<int>("quote_type");
    const auto defaultStyle = QStringLiteral("> \\1\n");
    const auto defaultRegex = QStringLiteral("(.+)(?:\n|$)");
    auto style = m_uiSettings.get<QString>("quote_style");
    auto regex = m_uiSettings.get<QString>("quote_regex");

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
                              const QString& selectedText, bool showingDetails)
{
    const auto modelIndex = m_messageModel->index(index, 0);
    const auto eventId = modelIndex.data(MessageEventModel::EventIdRole).toString();

    auto menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const auto* plEvt =
        currentRoom()->getCurrentState<Quotient::RoomPowerLevelsEvent>();
    const auto localUserId = currentRoom()->localUser()->id();
    const int userPl = plEvt->powerLevelForUser(localUserId);
    const auto* modelUser =
        modelIndex.data(MessageEventModel::AuthorRole).value<Quotient::User*>();
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
                        emit quote(modelIndex.data().toString());
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

void ChatRoomWidget::reactionButtonClicked(const QString& eventId, const QString& key)
{
    using namespace Quotient;
    const auto& annotations =
        currentRoom()->relatedEvents(eventId, EventRelation::Annotation());

    for (const auto& a : annotations)
        if (auto* e = eventCast<const ReactionEvent>(a);
            e != nullptr && e->relation().key == key
            && a->senderId() == currentRoom()->localUser()->id()) //
        {
            currentRoom()->redactEvent(a->id());
            return;
        }

    currentRoom()->postReaction(eventId, key);
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

    maybeReadTimer.start(m_uiSettings.get<int>("maybe_read_timer", 1000), this);
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
    if (readMarkerOnScreen && !indicesOnScreen.empty()
            && indexToMaybeRead < indicesOnScreen.back())
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
    return height() / 3;
}

void ChatRoomWidget::markShownAsRead()
{
    // FIXME: a case when a single message doesn't fit on the screen.
    if (auto room = currentRoom(); room != nullptr && readMarkerOnScreen) {
        const auto iter = room->findInTimeline(indicesOnScreen.back());
        Q_ASSERT(iter != room->timelineEdge());
        room->markMessagesAsRead((*iter)->id());
    }
}

bool ChatRoomWidget::pendingMarkRead() const
{
    if (!readMarkerOnScreen || !currentRoom())
        return false;

    const auto rm = currentRoom()->readMarker();
    return rm != currentRoom()->timelineEdge() && rm->index() < indexToMaybeRead;
}

void ChatRoomWidget::fileDrop(const QString& url)
{
    attachedFileName = QUrl(url).path();
    m_attachAction->setChecked(true);
    m_chatEdit->setPlaceholderText(
        tr("Add a message to the file or just push Enter"));
    emit showStatusMessage(tr("Attaching %1").arg(attachedFileName));
}

void ChatRoomWidget::htmlDrop(const QString &html)
{
    m_chatEdit->insertHtml(html);
}

void ChatRoomWidget::textDrop(const QString& text)
{
    m_chatEdit->insertPlainText(text);
}

Qt::KeyboardModifiers ChatRoomWidget::getModifierKeys() const
{
    return QGuiApplication::keyboardModifiers();
}
