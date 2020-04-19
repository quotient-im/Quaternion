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

#include "profiledialog.h"

#include <connection.h>
#include <csapi/device_management.h>

#include <QtWidgets/QHeaderView>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTableWidgetItem>

using Quotient::Connection;
using Quotient::BaseJob;

ProfileDialog::ProfileDialog(Connection *c, QWidget *parent)
    : Dialog(c->userId(), parent)
{
    tabWidget = new QTabWidget;

    auto tableWidget = new QTableWidget(this);
    tabWidget->addTab(tableWidget, "Devices");

    {
        tableWidget->setColumnCount(3);
        tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

        tableWidget->setHorizontalHeaderLabels(QStringList()
            << tr("Display Name")
            << tr("Device ID")
            << tr("Last Seen"));

        auto devicesJob = c->callApi<QMatrixClient::GetDevicesJob>();

        connect(devicesJob, &BaseJob::success, this, [=] {
            tableWidget->setRowCount(devicesJob->devices().size());

            for (int i = 0; i < devicesJob->devices().size(); i++) {
                auto device = devicesJob->devices()[i];
                auto name = new QTableWidgetItem(device.displayName);
                tableWidget->setItem(i, 0, name);

                auto id = new QTableWidgetItem(device.deviceId);
                tableWidget->setItem(i, 1, id);

                QDateTime lastSeen;
                lastSeen.setMSecsSinceEpoch(device.lastSeenTs.value_or(0));
                auto ip = new QTableWidgetItem(device.lastSeenIp
                    + " @ " + QLocale().toString(lastSeen, QLocale::ShortFormat));
                tableWidget->setItem(i, 2, ip);
            }
        });
    }

    addWidget(tabWidget);
}
