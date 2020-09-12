/*
 * Copyright (C) 2017 Elvis Angelaccio <elvis.angelaccio@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "kchatedit.h"

#include <QtCore/QDebug>
#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>

class KChatEdit::KChatEditPrivate
{
public:
    QString getDocumentText(QTextDocument* doc) const;
    void updateAndMoveInHistory(int increment);
    void saveInput();

    QTextDocument* makeDocument()
    {
        Q_ASSERT(contextKey);
        return new QTextDocument(contextKey);
    }

    void setContext(QObject* newContextKey)
    {
        contextKey = newContextKey;
        auto& context = contexts[contextKey]; // Create if needed
        auto& history = context.history;
        // History always ends with a placeholder that is initially empty
        // but may be filled with tentative input when the user entered
        // something and then went out for history.
        if (history.isEmpty() || !history.last()->isEmpty())
            history.push_back(makeDocument());

        while (history.size() > maxHistorySize)
            delete history.takeFirst();
        index = history.size() - 1;

        // QTextDocuments are parented to the context object, so are destroyed
        // automatically along with it; but the hashmap should be cleaned up
        if (newContextKey != q)
            QObject::connect(newContextKey, &QObject::destroyed, q,
                             [this, newContextKey] {
                                 contexts.remove(newContextKey);
                             });
        Q_ASSERT(contexts.contains(newContextKey) && !history.empty());
    }

    KChatEdit* q = nullptr;
    QObject* contextKey = nullptr;

    struct Context {
        QVector<QTextDocument*> history;
        QTextDocument* cachedInput = nullptr;
    };
    QHash<QObject*, Context> contexts;

    int index = 0;
    int maxHistorySize = 100;
};

QString KChatEdit::KChatEditPrivate::getDocumentText(QTextDocument* doc) const
{
    Q_ASSERT(doc);
    return q->acceptRichText() ? doc->toHtml() : doc->toPlainText();
}

void KChatEdit::KChatEditPrivate::updateAndMoveInHistory(int increment)
{
    Q_ASSERT(contexts.contains(contextKey));
    auto& history = contexts.find(contextKey)->history;
    Q_ASSERT(index >= 0 && index < history.size());
    if (index + increment < 0 || index + increment >= history.size())
        return; // Prevent stepping out of bounds
    auto& historyItem = history[index];

    // Only save input if different from the latest one.
    if (q->document() !=  historyItem /* shortcut expensive getDocumentText() */
        && getDocumentText(q->document()) != getDocumentText(historyItem))
        historyItem = q->document();

    // Fill the input with a copy of the history entry at a new index
    q->setDocument(history.at(index += increment)->clone(contextKey));
    q->moveCursor(QTextCursor::End);
}

void KChatEdit::KChatEditPrivate::saveInput()
{
    if (q->document()->isEmpty())
        return;

    Q_ASSERT(contexts.contains(contextKey));
    auto& history = contexts.find(contextKey)->history;
    // Only save input if different from the latest one or from the history.
    const auto input = getDocumentText(q->document());
    if (index < history.size() - 1
        && input == getDocumentText(history[index])) {
        // Take the history entry and move it to the most recent position (but
        // before the placeholder).
        history.move(index, history.size() - 2);
        emit q->savedInputChanged();
    } else if (input != getDocumentText(q->savedInput())) {
        // Insert a copy of the edited text just before the placeholder
        history.insert(history.end() - 1, q->document()->clone(contextKey));

        if (history.size() >= maxHistorySize) {
            delete history.takeFirst();
        }
        emit q->savedInputChanged();
    }

    index = history.size() - 1;
    q->clear();
}

KChatEdit::KChatEdit(QWidget *parent)
    : QTextEdit(parent), d(new KChatEditPrivate)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    connect(this, &QTextEdit::textChanged, this, &QWidget::updateGeometry);
    d->q = this; // KChatEdit initialization complete, pimpl can use it

    d->setContext(this); // A special context that always exists
    setDocument(d->makeDocument());
}

KChatEdit::~KChatEdit() = default;

QTextDocument* KChatEdit::savedInput() const
{
    Q_ASSERT(d->contexts.contains(d->contextKey));
    auto& history = d->contexts.find(d->contextKey)->history;
    if (history.size() >= 2)
        return history.at(history.size() - 2);

    Q_ASSERT(history.size() == 1);
    return history.front();
}

void KChatEdit::saveInput()
{
    d->saveInput();
}

QVector<QTextDocument*> KChatEdit::history() const
{
    Q_ASSERT(d->contexts.contains(d->contextKey));
    return d->contexts.value(d->contextKey).history;
}

int KChatEdit::maxHistorySize() const
{
    return d->maxHistorySize;
}

void KChatEdit::setMaxHistorySize(int newMaxSize)
{
    if (d->maxHistorySize != newMaxSize) {
        d->maxHistorySize = newMaxSize;
        emit maxHistorySizeChanged();
    }
}

void KChatEdit::switchContext(QObject* contextKey)
{
    if (!contextKey)
        contextKey = this;
    if (d->contextKey == contextKey)
        return;

    Q_ASSERT(d->contexts.contains(d->contextKey));
    d->contexts.find(d->contextKey)->cachedInput =
        document()->isEmpty() ? nullptr : document();
    d->setContext(contextKey);
    auto& cachedInput = d->contexts.find(d->contextKey)->cachedInput;
    setDocument(cachedInput ? cachedInput : d->makeDocument());
    moveCursor(QTextCursor::End);
    emit contextSwitched();
}

QSize KChatEdit::minimumSizeHint() const
{
    QSize minimumSizeHint = QTextEdit::minimumSizeHint();
    QMargins margins;
    margins += static_cast<int>(document()->documentMargin());
    margins += contentsMargins();

    if (!placeholderText().isEmpty()) {
        minimumSizeHint.setWidth(int(
            fontMetrics().boundingRect(placeholderText()).width()
            + margins.left()*2.5));
    }
    if (document()->isEmpty()) {
        minimumSizeHint.setHeight(fontMetrics().lineSpacing() + margins.top() + margins.bottom());
    } else {
        minimumSizeHint.setHeight(int(document()->size().height()));
    }

    return minimumSizeHint;
}

QSize KChatEdit::sizeHint() const
{
    ensurePolished();

    if (document()->isEmpty()) {
        return minimumSizeHint();
    }

    QMargins margins;
    margins += static_cast<int>(document()->documentMargin());
    margins += contentsMargins();

    QSize size = document()->size().toSize();
    size.rwidth() += margins.left() + margins.right();
    size.rheight() += margins.top() + margins.bottom();

    // Be consistent with minimumSizeHint().
    if (document()->lineCount() == 1 && !toPlainText().contains(QLatin1Char('\n'))) {
        size.setHeight(fontMetrics().lineSpacing() + margins.top() + margins.bottom());
    }

    return size;
}

void KChatEdit::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Copy)) {
        emit copyRequested();
        return;
    }

    switch (event->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
        if (!(QGuiApplication::keyboardModifiers() & Qt::ShiftModifier)) {
            emit returnPressed();
            return;
        }
        break;
    case Qt::Key_Up:
        if (!textCursor().movePosition(QTextCursor::Up)) {
            d->updateAndMoveInHistory(-1);
        }
        break;
    case Qt::Key_Down:
        if (!textCursor().movePosition(QTextCursor::Down)) {
            d->updateAndMoveInHistory(+1);
        }
        break;
    default:
        break;
    }

    QTextEdit::keyPressEvent(event);
}

