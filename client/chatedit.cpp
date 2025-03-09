/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2017 Kitsune Ral <kitsune-ral@users.sf.net>
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#include "chatedit.h"

#include "chatroomwidget.h"
#include "htmlfilter.h"
#include "timelinewidget.h"
#include "logging_categories.h"

#include <QtWidgets/QMenu>
#if QT_VERSION_MAJOR < 6
#include <QtWidgets/QShortcut>
#else
#include <QtGui/QShortcut>
#endif
#include <QtGui/QKeyEvent>
#include <QtCore/QMimeData>
#include <QtCore/QStringBuilder>
#include <QtCore/QDebug>

#include <Quotient/util.h>

static const QKeySequence ResetFormatShortcut("Ctrl+M");
static const QKeySequence AlternatePasteShortcut("Ctrl+Shift+V");

ChatEdit::ChatEdit(ChatRoomWidget* c)
    : KChatEdit(c), chatRoomWidget(c), matchesListPosition(0)
    , m_pastePlaintext(pastePlaintextByDefault())
{
    auto* sh = new QShortcut(this);
    sh->setKey(ResetFormatShortcut);
    connect(sh, &QShortcut::activated, this, &KChatEdit::resetCurrentFormat);

    sh = new QShortcut(this);
    sh->setKey(AlternatePasteShortcut);
    connect(sh, &QShortcut::activated, this, &ChatEdit::alternatePaste);
}

void ChatEdit::keyPressEvent(QKeyEvent* event)
{
    pickingMentions = false;
    if (event->key() == Qt::Key_Tab) {
        triggerCompletion();
        return;
    }

    cancelCompletion();
    KChatEdit::keyPressEvent(event);
}

void ChatEdit::contextMenuEvent(QContextMenuEvent *event)
{
    auto* menu = createStandardContextMenu();
    // The shortcut here is in order to show it to the user; it's the QShortcut
    // in the constructor that actually triggers on Ctrl+M (no idea
    // why the QAction doesn't work - because it's not in the main menu?)
    auto* action = new QAction(tr("Reset formatting"), this);
    action->setShortcut(ResetFormatShortcut);
    action->setStatusTip(tr("Reset the current character formatting to the default"));
    connect(action, &QAction::triggered, this, &KChatEdit::resetCurrentFormat);
    menu->addAction(action);

    action = new QAction(QIcon::fromTheme("edit-paste"),
                         pastePlaintextByDefault() ? tr("Paste as rich text")
                                                   : tr("Paste as plain text"),
                         this);
    action->setShortcut(AlternatePasteShortcut);
    connect(action, &QAction::triggered, this, &ChatEdit::alternatePaste);
    bool insert = false;
    for (QAction* a: menu->actions()) {
        if (insert) {
            menu->insertAction(a, action);
            break;
        }
        if (a->objectName() == QStringLiteral("edit-paste"))
            insert = true;
    }

    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(event->globalPos());
}

void ChatEdit::insertFromMimeData(const QMimeData* source) { acceptMimeData(source); }

void ChatEdit::switchContext(QObject* contextKey)
{
    cancelCompletion();
    KChatEdit::switchContext(contextKey);
}

bool ChatEdit::canInsertFromMimeData(const QMimeData* source) const
{
    if (!source)
        return false;
    // When not in a room, only allow dropping plain text (for commands)
    if (!chatRoomWidget->currentRoom())
        return source->hasText();

    return source->hasImage() || KChatEdit::canInsertFromMimeData(source);
}

void ChatEdit::dragEnterEvent(QDragEnterEvent* event)
{
    KChatEdit::dragEnterEvent(event);
    if (event->source() != this)
        chatRoomWidget->checkDndEvent(event);
}

void ChatEdit::alternatePaste()
{
    m_pastePlaintext = !pastePlaintextByDefault();
    paste();
    m_pastePlaintext = pastePlaintextByDefault();
}

bool ChatEdit::acceptMimeData(const QMimeData* source)
{
    if (!source) {
        qCWarning(MSGINPUT) << "Nothing to insert from the drop event";
        return true; // Treat it as nothing to do, not an error
    }

    if (source->hasImage())
        chatRoomWidget->attachImage(source->imageData().value<QImage>(), source->urls());
    else if (source->hasHtml()) {
        if (m_pastePlaintext) {
            QTextDocument document;
            document.setHtml(source->html());
            insertPlainText(document.toPlainText());
        } else {
            // Before insertion, remove formatting unsupported in Matrix
            const auto cleanHtml = chatRoomWidget->matrixHtmlFromMime(source);
            if (cleanHtml.isEmpty())
                return false;

            insertHtml(cleanHtml);
        }
        ensureCursorVisible();
    } else if (source->hasUrls()) {
        const auto& urls = source->urls();
        // Only the first local url is processed for now
        if (const auto urlIt = std::ranges::find_if(urls, &QUrl::isLocalFile); urlIt != urls.cend())
            chatRoomWidget->dropFile(urlIt->toLocalFile());
        else
            KChatEdit::insertFromMimeData(source);
    } else
        KChatEdit::insertFromMimeData(source);
    return true;
}

void ChatEdit::appendMentionAt(QTextCursor& cursor, QString mention,
                               QUrl mentionUrl, bool select)
{
    Q_ASSERT(!mention.isEmpty() && mentionUrl.isValid());
    if (cursor.atStart() && mention.startsWith('/'))
        mention.push_front('/');
    const auto posBeforeMention = cursor.position();
    const auto& safeMention = Quotient::sanitized(mention.toHtmlEscaped());
    // The most concise way to add a link is by QTextCursor::insertHtml()
    // as QTextDocument API is unwieldy (get to the block, make a fragment... -
    // just merging a char format with setAnchor()/setAnchorHref() doesn't work)
    if (Quotient::Settings().get("UI/hyperlink_users", true))
        cursor.insertHtml("<a href=\"" % mentionUrl.toEncoded() % "\">"
                          % safeMention % "</a>");
    else
        cursor.insertText(safeMention);
    cursor.setPosition(posBeforeMention, select ? QTextCursor::KeepAnchor
                                                : QTextCursor::MoveAnchor);
    ensureCursorVisible(); // The real one, not completionCursor
}

bool ChatEdit::initCompletion()
{
    completionCursor = textCursor();
    completionCursor.clearSelection();
    while (completionCursor.movePosition(QTextCursor::PreviousCharacter,
                                         QTextCursor::KeepAnchor)) {
        const auto& firstChar = completionCursor.selectedText().at(0);
        if (!firstChar.isLetterOrNumber() && firstChar != '@') {
            completionCursor.movePosition(QTextCursor::NextCharacter,
                                          QTextCursor::KeepAnchor);
            break;
        }
    }
    completionMatches =
        chatRoomWidget->findCompletionMatches(completionCursor.selectedText());
    if (completionMatches.isEmpty())
        return false;

    matchesListPosition = 0;
    // Add punctuation (either a colon and whitespace for salutations, or
    // just a whitespace for mentions) right away, in preparation for the cycle
    // of rotating completion matches (that are placed before this punctuation).
    auto punct = QStringLiteral(" ");
    static const QStringView ColonSpace = u": ";
    auto lookBehindCursor = completionCursor;
    if (lookBehindCursor.atStart())
        punct = ColonSpace.toString(); // Salutation
    else {
        for (auto i = 1; i <= ColonSpace.size(); ++i) {
            lookBehindCursor.movePosition(QTextCursor::PreviousCharacter,
                                          QTextCursor::KeepAnchor);
            if (lookBehindCursor.selectedText().startsWith(ColonSpace.left(i))) {
                // Replace the colon (with a following space if any found)
                // before the place of completion with a comma (with a huge
                // assumption that this colon ends a salutation).
                // The format is taken from the point of completion, to make
                // sure the inserted comma doesn't continue the format before
                // the colon.
                // TODO: use the fact that mentions are linkified now
                // to reliably detect salutations even to several users - but
                // take UI/hyperlink_users into account
                lookBehindCursor.insertText(QStringLiteral(", "),
                                            completionCursor.charFormat());
                punct = ColonSpace.toString();
                break;
            }
        }
    }
    const auto beforePunct = completionCursor.position();
    completionCursor.insertText(punct);
    completionCursor.setPosition(beforePunct);
    return true;
}

void ChatEdit::triggerCompletion()
{
    if (!isCompletionActive() && !initCompletion())
        return;

    Q_ASSERT(!completionMatches.empty()
             && matchesListPosition < completionMatches.size());
    const auto& completionMatch = completionMatches.at(matchesListPosition);
    appendMentionAt(completionCursor, completionMatch.first,
                    completionMatch.second, true);
    Q_ASSERT(!completionCursor.selectedText().isEmpty());
    auto completionHL = completionCursor.charFormat();
    completionHL.setUnderlineStyle(QTextCharFormat::DashUnderline);
    setExtraSelections({ { completionCursor, completionHL } });
    QStringList matchesForSignal;
    for (const auto& p: completionMatches)
        matchesForSignal.push_back(p.first);
    chatRoomWidget->showCompletions(matchesForSignal, matchesListPosition);
    matchesListPosition = (matchesListPosition + 1) % completionMatches.length();
}

void ChatEdit::cancelCompletion()
{
    completionMatches.clear();
    setExtraSelections({});
    Q_ASSERT(!isCompletionActive());

    emit cancelledCompletion();
}

bool ChatEdit::isCompletionActive() { return !completionMatches.isEmpty(); }

void ChatEdit::insertMention(QString author, QUrl url)
{
    // The order of inserting text below is such to be convenient for the user
    // to undo in case the primitive intelligence below fails.
    auto cursor = textCursor();
    // The mention may be hyperlinked, possibly changing the default
    // character format as a result if the mention happens to be at the end
    // of the block (which is almost always the case). So remember the format
    // at the point, and apply it later when printing the postfix.
    // triggerCompletion() doesn't have that problem because it inserts
    // the postfix before inserting the mention.
    auto textFormat = cursor.charFormat();
    appendMentionAt(cursor, author, url, false);

    // Add spaces and a colon around the inserted string if necessary.
    if (cursor.position() > 0 &&
            document()->characterAt(cursor.position() - 1).isLetterOrNumber())
        cursor.insertText(QStringLiteral(" "));

    while (cursor.movePosition(QTextCursor::PreviousCharacter) &&
           document()->characterAt(cursor.position()).isSpace());
    QString postfix;
    if (cursor.atStart())
        postfix = QStringLiteral(":");
    if ((pickingMentions || isCompletionActive())
        && document()->characterAt(cursor.position()) == ':') {
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        cursor.insertText(QStringLiteral(","));
        postfix = QStringLiteral(":");
    }
    auto editCursor = textCursor();
    auto currentChar = document()->characterAt(editCursor.position());
    if (editCursor.atBlockEnd() || currentChar.isLetterOrNumber()
        || currentChar == '.')
        postfix.push_back(' ');
    if (!postfix.isEmpty())
        editCursor.insertText(postfix, textFormat);
    pickingMentions = true;
    cancelCompletion();
}

bool ChatEdit::pastePlaintextByDefault()
{
    return Quotient::Settings().get("UI/paste_plaintext_by_default", true);
}
