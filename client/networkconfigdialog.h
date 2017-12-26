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

class QGroupBox;
class QButtonGroup;
class QLineEdit;
class QSpinBox;

class NetworkConfigDialog : public QDialog
{
        Q_OBJECT
    public:
        explicit NetworkConfigDialog(QWidget* parent = nullptr);
        ~NetworkConfigDialog();

    public slots:
        void applySettings();
        void loadSettings();
        void reactivate();

    private:
        template <typename T>
        using QObjectScopedPtr = QScopedPointer<T, QScopedPointerDeleteLater>;
        const QObjectScopedPtr<QGroupBox> useProxyBox;
        const QObjectScopedPtr<QButtonGroup> proxyTypeGroup;
        const QObjectScopedPtr<QLineEdit> proxyHostName;
        const QObjectScopedPtr<QSpinBox> proxyPort;
};
