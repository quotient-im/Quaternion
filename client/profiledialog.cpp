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

#include "accountselector.h"
#include "mainwindow.h"

#include <connection.h>
#include <user.h>
#include <room.h>
#include <csapi/device_management.h>

#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableWidgetItem>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
#include <QtCore/QStandardPaths>

using Quotient::BaseJob, Quotient::User, Quotient::Room;

void updateAvatarButton(Quotient::User* user, QPushButton* btn)
{
    const auto img = user->avatar(128);
    if (img.isNull()) {
        btn->setText(ProfileDialog::tr("No Avatar"));
        btn->setIcon({});
    } else {
        btn->setText({});
        btn->setIcon(QPixmap::fromImage(img));
        btn->setIconSize(img.size());
    }
}

ProfileDialog::ProfileDialog(AccountRegistry* accounts, MainWindow* parent)
    : Dialog(tr("User profiles"), parent)
    , m_settings("UI/ProfileDialog")
    , m_avatar(new QPushButton)
    , m_accountSelector(new AccountSelector(accounts))
    , m_displayName(new QLineEdit)
    , m_accessTokenLabel(new QLabel)
    , m_currentAccount(nullptr)
{
    Q_ASSERT(accounts != nullptr);
    auto* accountLayout = addLayout<QFormLayout>();
    accountLayout->addRow(tr("Account"), m_accountSelector);

    connect(m_accountSelector, &AccountSelector::currentAccountChanged, this,
            &ProfileDialog::load);
    connect(accounts, &AccountRegistry::aboutToDropAccount, this,
            [this, accounts] {
                if (accounts->size() == 1)
                    close(); // The last account is about to be dropped
            });

    auto cardLayout = addLayout<QHBoxLayout>();
    m_avatar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    cardLayout->addWidget(m_avatar, Qt::AlignLeft|Qt::AlignTop);

    connect(m_avatar, &QPushButton::clicked, this, [this] {
        const auto& dirs =
            QStandardPaths::standardLocations(QStandardPaths::PicturesLocation);
        auto* fDlg = new QFileDialog(this, tr("Set avatar"),
                                     dirs.isEmpty() ? QString() : dirs.back());
        fDlg->setFileMode(QFileDialog::ExistingFile);
        fDlg->setMimeTypeFilters({ "image/*", "application/octet-stream" });
        fDlg->open();
        connect(fDlg, &QFileDialog::fileSelected, this,
                [this](const QString& fileName) {
                    m_newAvatarPath = fileName;
                    if (!m_newAvatarPath.isEmpty()) {
                        auto img = QImage(m_newAvatarPath)
                                       .scaled(m_avatar->iconSize(),
                                               Qt::KeepAspectRatio);
                        m_avatar->setIcon(QPixmap(m_newAvatarPath));
                    }
                });
    });

    {
	    auto essentialsLayout = new QFormLayout();
	    essentialsLayout->addRow(tr("Display Name"), m_displayName);
	    auto accessTokenLayout = new QHBoxLayout();
        accessTokenLayout->addWidget(m_accessTokenLabel);
	    auto copyAccessToken = new QPushButton(tr("Copy to clipboard"));
        accessTokenLayout->addWidget(copyAccessToken);
        essentialsLayout->addRow(tr("Access token"), accessTokenLayout);
        cardLayout->addLayout(essentialsLayout);

        connect(copyAccessToken, &QPushButton::clicked, this, [this] {
            QGuiApplication::clipboard()->setText(account()->accessToken());
        });
	}

    static const QStringList deviceTableHeaders { tr("Device display name"),
                                                  tr("Device ID"),
                                                  tr("Last time seen"),
                                                  tr("Last IP address") };
    m_deviceTable = new QTableWidget(0, deviceTableHeaders.size());
    m_deviceTable->setHorizontalHeaderLabels(deviceTableHeaders);
    auto* headerCtl = m_deviceTable->horizontalHeader();
    headerCtl->setSectionResizeMode(QHeaderView::Interactive);
    headerCtl->setSectionsMovable(true);
    headerCtl->setSortIndicatorShown(true);
    m_deviceTable->verticalHeader()->setSectionResizeMode(
        QHeaderView::ResizeToContents);
    m_deviceTable->verticalHeader()->hide();
    m_deviceTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_deviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_deviceTable->setTabKeyNavigation(false);
    m_deviceTable->setSortingEnabled(true);
    m_deviceTable->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    addWidget(m_deviceTable);

    button(QDialogButtonBox::Ok)->setText(tr("Apply and close"));

    if (m_settings.contains("normal_geometry"))
        setGeometry(m_settings.value("normal_geometry").toRect());
}

ProfileDialog::~ProfileDialog()
{
    m_settings.setValue("normal_geometry", normalGeometry());
    m_settings.setValue("device_table_state",
                        m_deviceTable->horizontalHeader()->saveState());
    m_settings.sync();
}

void ProfileDialog::setAccount(Account* newAccount)
{
    m_accountSelector->setAccount(newAccount);
}

ProfileDialog::Account* ProfileDialog::account() const
{
    return m_currentAccount;
}

inline auto* makeTableItem(const QString& text,
                           Qt::ItemFlags addFlags = Qt::NoItemFlags)
{
    auto* item = new QTableWidgetItem(text);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | addFlags);
    return item;
}

void ProfileDialog::load()
{
    if (m_currentAccount)
        disconnect(m_currentAccount->user(), nullptr, this, nullptr);
    m_deviceTable->clearContents();
    m_avatar->setText(tr("No avatar"));
    m_avatar->setIcon({});
    m_displayName->clear();
    m_accessTokenLabel->clear();

    m_currentAccount = m_accountSelector->currentAccount();
    if (!m_currentAccount)
        return;

    auto* user = m_currentAccount->user();
    updateAvatarButton(user, m_avatar);
    connect(user, &User::avatarChanged, this, [this](User*, const Room* room) {
        if (!room)
            updateAvatarButton(account()->user(), m_avatar);
    });

    m_displayName->setText(user->name());
    m_displayName->setFocus();
    connect(user, &User::nameChanged, this,
            [this](const QString& newName, auto, const Room* room) {
                if (!room)
                    m_displayName->setText(newName);
            });

    auto accessToken = account()->accessToken();
    if (Q_LIKELY(accessToken.size() > 10))
        accessToken.replace(5, accessToken.size() - 10, "...");
    m_accessTokenLabel->setText(accessToken);

    m_deviceTable->setRowCount(1);
    m_deviceTable->setItem(0, 0, new QTableWidgetItem(tr("Loading...")));
    auto devicesJob = m_currentAccount->callApi<Quotient::GetDevicesJob>();
    // TODO: Give some feedback while the thing is loading
    connect(devicesJob, &BaseJob::success, this, [this, devicesJob] {
        m_devices = devicesJob->devices();
        m_deviceTable->setRowCount(m_devices.size());

        for (int i = 0; i < m_devices.size(); ++i) {
            auto device = m_devices[i];

            m_deviceTable->setItem(
                i, 0, makeTableItem(device.displayName, Qt::ItemIsEditable));
            m_deviceTable->setItem(i, 1, makeTableItem(device.deviceId));
            if (device.lastSeenTs) {
                const auto& lastSeen =
                    QDateTime::fromMSecsSinceEpoch(*device.lastSeenTs);
                m_deviceTable->setItem(i, 2,
                                       makeTableItem(QLocale().toString(
                                           lastSeen, QLocale::ShortFormat)));
            }
            m_deviceTable->setItem(i, 3, makeTableItem(device.lastSeenIp));
        }

        if (m_settings.contains("device_table_state"))
            m_deviceTable->horizontalHeader()->restoreState(
                m_settings.value("device_table_state").toByteArray());
        else {
            // Initialise the state
            m_deviceTable->sortByColumn(2, Qt::DescendingOrder);
            m_deviceTable->resizeColumnsToContents();
        }
    });
}

void ProfileDialog::apply()
{
    if (!m_currentAccount) {
        qWarning() << "ProfileDialog: no account chosen, can't apply changes";
        return;
    }
    auto* user = m_currentAccount->user();
    if (m_displayName->text() != user->name())
        user->rename(m_displayName->text());
    if (!m_newAvatarPath.isEmpty())
        user->setAvatar(m_newAvatarPath);

    for (const auto& device: m_devices) {
        const auto& list =
            m_deviceTable->findItems(device.deviceId, Qt::MatchExactly);
        if (list.empty())
            continue;
        const auto& newName = m_deviceTable->item(list[0]->row(), 0)->text();
        if (!list.isEmpty() && newName != device.displayName)
            m_currentAccount->callApi<Quotient::UpdateDeviceJob>(device.deviceId,
                                                                 newName);
    }
    accept();
}
