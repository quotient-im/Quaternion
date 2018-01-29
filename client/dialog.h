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

#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QVBoxLayout>

class QAbstractButton;
class QLabel;

class Dialog : public QDialog
{
        Q_OBJECT
    public:
        enum ApplyLatency { InstantApply, LongApply };
        static const auto NoExtraButtons = QDialogButtonBox::NoButton;

        explicit Dialog(const QString& title, QWidget *parent = nullptr,
            ApplyLatency applyLatency = InstantApply,
            const QString& applyTitle = {},
            QDialogButtonBox::StandardButtons addButtons = QDialogButtonBox::Reset);

        template <typename LayoutT>
        LayoutT* addLayout()
        {
            auto l = new LayoutT;
            addLayout(l);
            return l;
        }
        void addLayout(QLayout* l);
        void addWidget(QWidget* w);

    public slots:
        void reactivate();
        void setStatusMessage(const QString& msg);
        void applyFailed(const QString& errorMessage);

    protected:
        virtual void load() { }
        virtual void apply() { accept(); }
        virtual void buttonClicked(QAbstractButton* button);

        QDialogButtonBox* buttonBox() const { return buttons; }
        QLabel* statusLine() const { return statusLabel; }

        void setPendingApplyMessage(const QString& msg)
        { pendingApplyMessage = msg; }

    private:
        ApplyLatency applyLatency;
        QString pendingApplyMessage;

        QDialogButtonBox* buttons;
        QLabel* statusLabel;

        QVBoxLayout outerLayout;
};
