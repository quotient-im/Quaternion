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

#include <QDebug>
#include <QGuiApplication>
#include <QKeyEvent>

class KChatEdit::KChatEditPrivate
{
public:
    KChatEditPrivate(KChatEdit *parent)
        : q(parent)
        , index(0)
        , maxHistorySize(100)
    {}

    void init();
    void emitReturnPressed();
    void rewindHistory();
    void forwardHistory();
    void saveInput();

    KChatEdit *q;
    QVector<QTextDocument*> history;
    int index;
    int maxHistorySize;
};

void KChatEdit::KChatEditPrivate::init()
{
    // History always ends with a dummy placeholder string.
    history << new QTextDocument(q);
}

void KChatEdit::KChatEditPrivate::emitReturnPressed()
{
    if (q->document()->isEmpty()) {
        return;
    }

    emit q->returnPressed();
}

void KChatEdit::KChatEditPrivate::rewindHistory()
{
    history[index] = q->document()->clone(q);

    // History finished, nothing to do.
    if (index == 0) {
        return;
    }

    index--;
    q->setDocument(history.at(index));
    q->moveCursor(QTextCursor::End);
}

void KChatEdit::KChatEditPrivate::forwardHistory()
{
    history[index] = q->document()->clone(q);

    // Back from history, nothing to do.
    if (index == history.size() - 1) {
        return;
    }

    index++;
    q->setDocument(history.at(index));
    q->moveCursor(QTextCursor::End);
}

void KChatEdit::KChatEditPrivate::saveInput()
{
    if (q->document()->isEmpty()) {
        return;
    }

    const QString input = q->acceptRichText() ? q->toHtml() : q->toPlainText();
    const QString latestInput = q->acceptRichText() ? q->savedInput()->toHtml() : q->savedInput()->toPlainText();
    // Only save input if different from the latest one.
    if (input != latestInput) {
        // Replace empty placeholder with the new input.
        history[history.size() - 1] = q->document()->clone();
        history << new QTextDocument(q);

        if(history.size() > maxHistorySize) {
            history.removeFirst();
        }
    }

    index = history.size() - 1;
    emit q->savedInputChanged();
    q->clear();
}

KChatEdit::KChatEdit(QWidget *parent)
    : QTextEdit(parent), d(new KChatEditPrivate(this))
{
    d->init();
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    connect(this, &QTextEdit::textChanged, this, &QWidget::updateGeometry);
}

KChatEdit::~KChatEdit()
{
}

QTextDocument* KChatEdit::savedInput() const
{
    if (d->history.size() >= 2)
        return d->history.at(d->history.size() - 2);

    Q_ASSERT(d->history.size() == 1);
    return d->history.last();
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
        d->history << new QTextDocument(this);
    }

    while (d->history.size() > maxHistorySize()) {
        d->history.removeFirst();
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
        minimumSizeHint.setWidth(fontMetrics().width(placeholderText()) + margins.left()*2.5);
    }
    if (document()->isEmpty()) {
        minimumSizeHint.setHeight(fontMetrics().lineSpacing() + margins.top() + margins.bottom());
    } else {
        minimumSizeHint.setHeight(document()->size().height());
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
    switch (event->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
        if (!(QGuiApplication::keyboardModifiers() & Qt::ShiftModifier)) {
            d->emitReturnPressed();
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

