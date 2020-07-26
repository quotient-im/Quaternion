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
#include <room.h>
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

using Quotient::BaseJob;

ProfileDialog::ProfileDialog(Quotient::User* u, QWidget* parent)
    : Dialog(u->id(), parent)
    , m_user(u)
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
            connect(m_user, &Quotient::User::avatarChanged, this,
                    [=] (Quotient::User* user, const Quotient::Room* room) {
                if (room)
                    return;

                QImage img = user->avatar(64);
                if (img.isNull())
                    m_avatar->setText(tr("No Avatar"));
                else
                    m_avatar->setPixmap(QPixmap::fromImage(img));
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

    m_deviceTable = new QTableWidget;
    {
        m_deviceTable->setColumnCount(3);
        m_deviceTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_deviceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        m_deviceTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        m_deviceTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        m_deviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);

        m_deviceTable->setHorizontalHeaderLabels(QStringList()
            << tr("Display Name")
            << tr("Device ID")
            << tr("Last Seen"));
    }
    tabWidget->addTab(m_deviceTable, tr("Devices"));

    addWidget(tabWidget);
}

void ProfileDialog::load()
{
    auto avatar = m_user->avatar(64);
    if (avatar.isNull())
        m_avatar->setText(tr("No Avatar"));
    else
        m_avatar->setPixmap(QPixmap::fromImage(avatar));
    m_userId->setText(m_user->id());
    m_displayName->setText(m_user->displayname());

    auto devicesJob = m_user->connection()->callApi<Quotient::GetDevicesJob>();
    connect(devicesJob, &BaseJob::success, this, [=] {
        m_deviceTable->setRowCount(devicesJob->devices().size());

        for (int i = 0; i < devicesJob->devices().size(); ++i) {
            auto device = devicesJob->devices()[i];
            m_devices[device.deviceId] = device.displayName;

            auto name = new QTableWidgetItem(device.displayName);
            m_deviceTable->setItem(i, 0, name);

            auto id = new QTableWidgetItem(device.deviceId);
            id->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            m_deviceTable->setItem(i, 1, id);

            QDateTime lastSeen;
            lastSeen.setMSecsSinceEpoch(device.lastSeenTs.value_or(0));
            auto ip = new QTableWidgetItem(device.lastSeenIp
                + " @ " + QLocale().toString(lastSeen, QLocale::ShortFormat));
            ip->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            m_deviceTable->setItem(i, 2, ip);
       }
    });
}

void ProfileDialog::apply()
{
    if (m_displayName->text() != m_user->displayname())
        m_user->rename(m_displayName->text());
    if (!m_avatarUrl.isEmpty())
        m_user->setAvatar(m_avatarUrl);

    for (auto deviceIt = m_devices.cbegin(); deviceIt != m_devices.cend(); ++deviceIt) {
        auto list = m_deviceTable->findItems(deviceIt.key(), Qt::MatchExactly);
        auto newName = m_deviceTable->item(list[0]->row(), 0)->text();
        if (!list.isEmpty() && newName != deviceIt.value())
            m_user->connection()->callApi<Quotient::UpdateDeviceJob>(deviceIt.key(), newName);
    }
    accept();
}
