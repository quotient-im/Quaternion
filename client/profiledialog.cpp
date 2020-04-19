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
#include <user.h>
#include <csapi/device_management.h>

#include <QtWidgets/QAction>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTableWidgetItem>

using Quotient::Connection;
using Quotient::BaseJob;

ProfileDialog::ProfileDialog(Connection *c, QWidget *parent)
    : Dialog(c->userId(), parent)
    , m_user(c->user())
    , tabWidget(new QTabWidget)
    , m_avatar(new QLabel)
    , m_userId(new QLabel)
    , m_displayName(new QLineEdit)
{
    auto profileWidget = new QWidget(this);
    {
        auto topLayout = new QHBoxLayout;
        {
            auto avatarLayout = new QVBoxLayout;
            m_avatar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
            m_avatar->setPixmap({64, 64});
            avatarLayout->addWidget(m_avatar);

            auto uploadButton = new QPushButton(tr("Set Avatar..."));
            connect(uploadButton, &QPushButton::clicked, this, [this] {
                m_avatarUrl =
                    QFileDialog::getOpenFileName(this, tr("Set avatar"),
                        {}, tr("Images (*.png *.jpg)"));
                if (!m_avatarUrl.isEmpty()) {
                    QImage img = QImage(m_avatarUrl).scaled({64, 64}, Qt::KeepAspectRatio);
                    m_avatar->setPixmap(QPixmap::fromImage(img));
                }
            });

            avatarLayout->addWidget(uploadButton);
            topLayout->addLayout(avatarLayout);
        }

        {
            auto essentialsLayout = new QFormLayout;
            essentialsLayout->addRow(tr("Account"), m_userId);
            essentialsLayout->addRow(tr("Display Name"), m_displayName);
            topLayout->addLayout(essentialsLayout);
        }

        profileWidget->setLayout(topLayout);
    }
    tabWidget->addTab(profileWidget, tr("Profile"));

    auto tableWidget = new QTableWidget(this);
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
    tabWidget->addTab(tableWidget, tr("Devices"));

    addWidget(tabWidget);
}

void ProfileDialog::load()
{
    m_avatar->setPixmap(QPixmap::fromImage(m_user->avatar(64)));
    m_userId->setText(m_user->id());
    m_displayName->setText(m_user->displayname());
}

void ProfileDialog::apply()
{
    if (m_displayName->text() != m_user->displayname())
        m_user->rename(m_displayName->text());
    if (!m_avatarUrl.isEmpty())
        m_user->setAvatar(m_avatarUrl);
    accept();
}
