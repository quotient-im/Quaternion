/******************************************************************************
 * Copyright (C) 2017 Kitsune Ral <kitsune-ral@users.sf.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include <QtCore/QFlags>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QVBoxLayout>

class QAbstractButton;
class QLabel;

class Dialog : public QDialog
{
        Q_OBJECT
    public:
        enum UseStatusLine { NoStatusLine, StatusLine };
        static const auto NoExtraButtons = QDialogButtonBox::NoButton;

        explicit Dialog(const QString& title, QWidget *parent = nullptr,
            UseStatusLine useStatusLine = NoStatusLine,
            const QString& applyTitle = {},
            QDialogButtonBox::StandardButtons addButtons = QDialogButtonBox::Reset);

        explicit Dialog(const QString& title, QDialogButtonBox::StandardButtons setButtons,
            QWidget *parent = nullptr,
            UseStatusLine useStatusLine = NoStatusLine);

        /// Create and add a layout of the given type
        /*! This creates a new layout object and adds it to the bottom of
         * the dialog client area (i.e., above the button box). */
        template <typename LayoutT>
        LayoutT* addLayout()
        {
            auto l = new LayoutT;
            addLayout(l);
            return l;
        }
        /// Add a layout to the bottom of the dialog's client area
        void addLayout(QLayout* l);
        /// Add a widget to the bottom of the dialog's client area
        void addWidget(QWidget* w);

        QPushButton* button(QDialogButtonBox::StandardButton which);

    public slots:
        /// Show or raise the dialog
        void reactivate();
        /// Set the status line of the dialog window
        void setStatusMessage(const QString& msg);
        /// Return to the dialog after a failed apply
        void applyFailed(const QString& errorMessage);

    protected:
        /// (Re-)Load data in the dialog
        /*! \sa buttonClicked */
        virtual void load() { }
        /// Check data in the dialog before accepting
        /*! \sa apply, buttonClicked */
        virtual bool validate() { return true; }
        /// Apply changes and close the dialog
        /*!
         * This method is invoked upon clicking the "apply" button (by default
         * it's the one with `AcceptRole`), if validate() returned true.
         * \sa buttonClicked, validate
         */
        virtual void apply() { accept(); }
        /// React to a click of a button in the dialog box
        /*!
         * This virtual function is invoked every time one of push buttons
         * in the dialog button box is clicked; it defines how the dialog reacts
         * to each button. By default, it calls validate() and, if it succeeds,
         * apply() on buttons with `AcceptRole`; cancels the dialog on
         * `RejectRole`; and reloads the fields on `ResetRole`. Override this
         * method to change this behaviour.
         * \sa validate, apply, reject, load
         */
        virtual void buttonClicked(QAbstractButton* button);

        QDialogButtonBox* buttonBox() const { return buttons; }
        QLabel* statusLine() const { return statusLabel; }

        void setPendingApplyMessage(const QString& msg)
        { pendingApplyMessage = msg; }

    private:
        UseStatusLine applyLatency;
        QString pendingApplyMessage;

        QLabel* statusLabel;
        QDialogButtonBox* buttons;

        QVBoxLayout outerLayout;
};
