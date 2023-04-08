/*
 * SPDX-FileCopyrightText: 2017 Elvis Angelaccio <elvis.angelaccio@kde.org>
 * SPDX-FileCopyrightText: 2020 The Quotient project
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QtWidgets/QTextEdit>

/**
 * @class KChatEdit kchatedit.h KChatEdit
 *
 * @brief An input widget with history for chat applications.
 *
 * This widget can be used to get input for chat windows, which typically
 * corresponds to chat messages or protocol-specific commands (for example the
 * "/whois" IRC command).
 *
 * By default the widget takes as little space as possible, which is the same
 * space as used by a QLineEdit. It is possible to expand the widget and enter
 * "multi-line" messages, by pressing Shift + Return.
 *
 * Chat applications usually maintain a history of what the user typed, which
 * can be browsed with the Up and Down keys (exactly like in command-line
 * shells). This feature is fully supported by this widget. The widget emits the
 * inputRequested() signal upon pressing the Return key. You can then call
 * saveInput() to make the input text disappear, as typical in chat
 * applications. The input goes in the history and can be retrieved with the
 * savedInput() method.
 *
 * @author Elvis Angelaccio <elvis.angelaccio@kde.org>
 * @author Kitsune Ral <kitsune-ral@users.sf.net>
 */
class KChatEdit : public QTextEdit
{
    Q_OBJECT
    Q_PROPERTY(QTextDocument* savedInput READ savedInput NOTIFY savedInputChanged)
    Q_PROPERTY(int maxHistorySize READ maxHistorySize WRITE setMaxHistorySize NOTIFY maxHistorySizeChanged)

public:
    explicit KChatEdit(QWidget *parent = nullptr);
    ~KChatEdit() override;

    /**
     * The latest input text saved in the history.
     * This corresponds to the last element of history().
     * @return Latest available input or an empty document if saveInput() has not been called yet.
     * @see inputChanged(), saveInput(), history()
     */
    QTextDocument* savedInput() const;

    /**
     * Saves in the history the current document().
     * This also clears the QTextEdit area.
     * @note If the history is full (see maxHistorySize(), new inputs will take space from the oldest
     * items in the history.
     * @see savedInput(), history(), maxHistorySize()
     */
    void saveInput();

    /**
     * @return The history of the text inputs that the user typed.
     * @see savedInput(), saveInput();
     */
    QVector<QTextDocument*> history() const;

    /**
     * @return The maximum number of input items that the history can store.
     * @see history()
     */
    int maxHistorySize() const;

    /**
     * Set the maximum number of input items that the history can store.
     * @see maxHistorySize()
     */
    void setMaxHistorySize(int newMaxSize);

    QSize minimumSizeHint() const Q_DECL_OVERRIDE;
    QSize sizeHint() const Q_DECL_OVERRIDE;

public Q_SLOTS:
    /**
     * @brief Switch the context (e.g., a chat room) of the widget
     *
     * This clears the current entry and the history of the chat edit
     * and replaces them with the entry and the history for the object
     * passed as a parameter, if there are any.
     */
    virtual void switchContext(QObject* contextKey);

    /**
     * @brief Reset the current character(s) formatting
     *
     * This is equivalent to calling `setCurrentCharFormat({})`.
     */
    void resetCurrentFormat();

Q_SIGNALS:
    /**
     * A new input has been saved in the history.
     * @see savedInput(), saveInput(), history()
     */
    void savedInputChanged();

    /**
     * Emitted when the user types Key_Return or Key_Enter, which typically means the user
     * wants to "send" what was typed. Call saveInput() if you want to actually save the input.
     * @see savedInput(), saveInput(), history()
     */
    void returnPressed();

    /**
     * Emitted when the user presses Ctrl+C.
     */
    void copyRequested();

    /** A new context has been selected */
    void contextSwitched();

    void maxHistorySizeChanged();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    class KChatEditPrivate;
    QScopedPointer<KChatEditPrivate> d;

    Q_DISABLE_COPY(KChatEdit)
};
