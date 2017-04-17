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

#ifndef KCHATEDIT_H
#define KCHATEDIT_H

#include <QTextEdit>

/**
 * @class KChatEdit kchatedit.h KChatEdit
 *
 * @brief An input widget with history for chat applications.
 *
 * This widget can be used to get input for chat windows, which tipically corresponds to chat messages or
 * protocol-specific commands (for example the "/whois" IRC command).
 *
 * By default the widget takes as less space as possible, which is the same space as used by a QLineEdit.
 * It is possible to expand the widget and enter "multi-line" messages, by pressing Shift + Return.
 *
 * Chat applications usually maintain a history of what the user typed, which can be browsed with the
 * Up and Down keys (exactly like in command-line shells). This feature is fully supported by this widget.
 * The widget emits the inputChanged() signal upon pressing the Return key.
 * You can then call saveInput() to make the input text disappear, as typical in chat applications.
 * The input goes in the history and can be retrieved with the input() method.
 *
 * @author Elvis Angelaccio <elvis.angelaccio@kde.org>
 */
class KChatEdit : public QTextEdit
{
    Q_OBJECT
    Q_PROPERTY(QString input READ input NOTIFY inputChanged)
    Q_PROPERTY(QStringList history READ history WRITE setHistory)
    Q_PROPERTY(int maxHistorySize READ maxHistorySize WRITE setMaxHistorySize)

public:
    explicit KChatEdit(QWidget *parent = nullptr);
    virtual ~KChatEdit();

    /**
     * The latest input text typed by the user.
     * This corresponds to the last element of history().
     * @return Latest available input or an empty string if saveInput() has not been called yet.
     * @see inputChanged(), saveInput(), history()
     */
    QString input() const;

    /**
     * Saves in the history the text typed before pressing the Return key.
     * This also clears the QTextEdit area.
     * @note If the history is full (see maxHistorySize(), new inputs will take space from the oldest
     * items in the history.
     * @see input(), history(), maxHistorySize()
     */
    void saveInput();

    /**
     * @return The history of the text inputs that the user typed.
     * @see input(), saveInput();
     */
    QStringList history() const;

    /**
     * Set the history of this widget to @p history.
     * This can be useful when sharing a single instance of KChatEdit with many "channels" or "rooms"
     * that maintain their own private history.
     */
    void setHistory(const QStringList &history);

    /**
     * @return The maximum number of input items that the history can store.
     * @see history()
     */
    int maxHistorySize() const;

    /**
     * Set the maximum number of input items that the history can store.
     * @see maxHistorySize()
     */
    void setMaxHistorySize(int maxHistorySize);

    QSize minimumSizeHint() const Q_DECL_OVERRIDE;
    QSize sizeHint() const Q_DECL_OVERRIDE;

Q_SIGNALS:
    /**
     * The user has typed @p input and then pressed the Return key.
     * Call saveInput() if you want to push @p input to the history.
     * @see input(), saveInput(), history()
     */
    void inputChanged(const QString &input);

protected:
    void keyPressEvent(QKeyEvent *event) Q_DECL_OVERRIDE;

private:
    class KChatEditPrivate;
    QScopedPointer<KChatEditPrivate> d;

    Q_DISABLE_COPY(KChatEdit)
};

#endif
