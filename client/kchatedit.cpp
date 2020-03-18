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
#include <settings.h>

#include <QDebug>
#include <QGuiApplication>
#include <QKeyEvent>

static inline QTextDocument* makeDocument()
{
    return new QTextDocument;
}

class KChatEdit::KChatEditPrivate
{
public:
    QString getDocumentText(QTextDocument* doc) const;
    void updateAndMoveInHistory(int increment);
    void rewindHistory();
    void forwardHistory();
    void saveInput();

    KChatEdit *q = nullptr;
    // History always ends with a placeholder string that is initially empty
    // but may be filled with tentative input when the user entered something
    // and then went out for history.
    QVector<QTextDocument*> history { 1, makeDocument() };
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
    Q_ASSERT(index >= 0 && index < history.size());
    // Only save input if different from the latest one.
    if (getDocumentText(q->document()) != getDocumentText(history[index]))
    {
        history[index] = q->document();
        history[index]->setParent(nullptr);
    }

    const auto* nextDocument = history.at(index += increment);
    q->setDocument(nextDocument->clone(q));
    q->moveCursor(QTextCursor::End);
}

void KChatEdit::KChatEditPrivate::rewindHistory()
{
    if (index > 0)
        updateAndMoveInHistory(-1);
}

void KChatEdit::KChatEditPrivate::forwardHistory()
{
    if (index < history.size() - 1)
        updateAndMoveInHistory(+1);
}

void KChatEdit::KChatEditPrivate::saveInput()
{
    if (q->document()->isEmpty()) {
        return;
    }

    // Only save input if different from the latest one or from the history.
    const auto input = getDocumentText(q->document());
    if (index < history.size() - 1 &&
            input == getDocumentText(history[index])) {
        // Take the history entry and move it to the most recent position (but
        // before the placeholder).
#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
        history.move(index, history.size() - 2);
#else
        history.insert(history.size() - 2, history.takeAt(index));
#endif
        emit q->savedInputChanged();
    } else if (input != getDocumentText(q->savedInput())) {
        // Replace the placeholder with the new input.
        history.back() = q->document()->clone();

        if (history.size() >= maxHistorySize) {
            delete history.takeFirst();
        }
        // Make a new placeholder.
        history << makeDocument();
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

    setDocument(makeDocument());
}

KChatEdit::~KChatEdit() = default;

QTextDocument* KChatEdit::savedInput() const
{
    if (d->history.size() >= 2)
        return d->history.at(d->history.size() - 2);

    Q_ASSERT(d->history.size() == 1);
    return d->history.front();
}

void KChatEdit::saveInput()
{
    d->saveInput();
}

QVector<QTextDocument*> KChatEdit::history() const
{
    return d->history;
}

void KChatEdit::setHistory(const QVector<QTextDocument*> &history)
{
    d->history = history;
    if (history.isEmpty() || !history.last()->isEmpty()) {
        d->history << makeDocument();
    }

    while (d->history.size() > maxHistorySize()) {
        delete d->history.takeFirst();
    }

    d->index = d->history.size() - 1;
}

int KChatEdit::maxHistorySize() const
{
    return d->maxHistorySize;
}

void KChatEdit::setMaxHistorySize(int maxHistorySize)
{
    d->maxHistorySize = maxHistorySize;
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
            d->rewindHistory();
        }
        break;
    case Qt::Key_Down:
        if (!textCursor().movePosition(QTextCursor::Down)) {
            d->forwardHistory();
        }
        break;
    default:
        break;
    }

    QTextEdit::keyPressEvent(event);
}

