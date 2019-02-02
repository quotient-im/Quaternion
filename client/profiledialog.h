/**************************************************************************
 *                                                                        *
 * Copyright (C) 2019 Karol Kosek <krkkx@protonmail.com>                  *
 *                                                                        *
 * This program is free software; you can redistribute it and/or          *
 * modify it under the terms of the GNU General Public License            *
 * as published by the Free Software Foundation; either version 3         *
 * of the License, or (at your option) any later version.                 *
 *                                                                        *
 * This program is distributed in the hope that it will be useful,        *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 * GNU General Public License for more details.                           *
 *                                                                        *
 * You should have received a copy of the GNU General Public License      *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 *                                                                        *
 **************************************************************************/

#pragma once

#include "dialog.h"

class QDialogButtonBox;
class QLineEdit;
class QTabWidget;

namespace Quotient {
    class Connection;
}

class ProfileDialog : public QDialog
{
        Q_OBJECT

    public:
        explicit ProfileDialog(Quotient::Connection *c, QWidget *parent = 0);

    private:
        QTabWidget *tabWidget;
        QDialogButtonBox *buttonBox;
};
