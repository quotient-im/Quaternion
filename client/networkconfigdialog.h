/*
 * SPDX-FileCopyrightText: 2017 Kitsune Ral <kitsune-ral@users.sf.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "dialog.h"

class QGroupBox;
class QButtonGroup;
class QLineEdit;
class QSpinBox;

class NetworkConfigDialog : public Dialog
{
        Q_OBJECT
    public:
        explicit NetworkConfigDialog(QWidget* parent = nullptr);
        ~NetworkConfigDialog();

    private slots:
        void apply() override;
        void load() override;
        void maybeDisableControls();

    private:
        QGroupBox* useProxyBox;
        QButtonGroup* proxyTypeGroup;
        QLineEdit* proxyHostName;
        QSpinBox* proxyPort;
        QLineEdit* proxyUserName;
};
