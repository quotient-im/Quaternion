/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2019 Karol Kosek <krkkx@protonmail.com>        *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#include "profiledialog.h"

#include "accountselector.h"
#include "mainwindow.h"

#include <Quotient/connection.h>
#include <Quotient/user.h>
#include <Quotient/room.h>
#include <Quotient/csapi/device_management.h>

#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableWidgetItem>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>

#include <QtCore/QStandardPaths>

using Quotient::BaseJob, Quotient::User, Quotient::Room;

class TimestampTableItem : public QTableWidgetItem {
public:
    explicit TimestampTableItem(const QDateTime& timestamp)
        : QTableWidgetItem(QLocale().toString(timestamp, QLocale::ShortFormat),
                           UserType)
    {
        setData(Qt::UserRole, timestamp);
    }
    explicit TimestampTableItem(const TimestampTableItem& other) = default;
    ~TimestampTableItem() override = default;
    void operator=(const TimestampTableItem& other) = delete;
    TimestampTableItem* clone() const override
    {
        return new TimestampTableItem(*this);
    }

    bool operator<(const QTableWidgetItem& other) const override
    {
        return other.type() != UserType
                   ? QTableWidgetItem::operator<(other)
                   : data(Qt::UserRole).value<QDateTime>()
                         < other.data(Qt::UserRole).value<QDateTime>();
    }
};

/*! Device table class
 *
 * Encapsulates the columns model and formatting
 */
class ProfileDialog::DeviceTable : public QTableWidget {
public:
    enum Columns : int {
        DeviceName,
        DeviceId,
        LastTimeSeen,
        LastIpAddr
    };
    DeviceTable();
    ~DeviceTable() override = default;

    template <Columns ColumnN>
    using ItemType = std::conditional_t<ColumnN == LastTimeSeen,
                                        TimestampTableItem, QTableWidgetItem>;

    template <Columns ColumnN>
    static inline constexpr auto itemFlags =
        Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled
        | Qt::ItemFlag((ColumnN == DeviceName) & Qt::ItemIsEditable);

    using QTableWidget::setItem;
    template <Columns ColumnN, typename DataT>
    inline auto setItem(int row, const DataT& data)
        -> std::enable_if_t<std::is_constructible_v<ItemType<ColumnN>, DataT>>
    {
        auto* item = new ItemType<ColumnN>(data);
        item->setFlags(itemFlags<ColumnN>);
        setItem(row, ColumnN, item);
    }

    void markupRow(int row, void (QFont::*fontFn)(bool),
                   const QString& rowToolTip, bool flagValue = true);

    void markCurrentRow(int row) {
        markupRow(row, &QFont::setBold, tr("This is the current device"));
    }

    void fillPendingData(const QString& currentDeviceId);
    void refresh(const QVector<Quotient::Device>& devices, const QString &currentDeviceId);
};

ProfileDialog::DeviceTable::DeviceTable()
{
    static const QStringList Headers {
        // Must be synchronised with DeviceTable::Columns
        tr("Device display name"), tr("Device ID"),
        tr("Last time seen"), tr("Last IP address")
    };

    setColumnCount(Headers.size());
    setHorizontalHeaderLabels(Headers);
    auto* headerCtl = horizontalHeader();
    headerCtl->setSectionResizeMode(QHeaderView::Interactive);
    headerCtl->setSectionsMovable(true);
    headerCtl->setFirstSectionMovable(false);
    headerCtl->setSortIndicatorShown(true);
    verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    verticalHeader()->hide();
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setTabKeyNavigation(false);
    setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
}

void updateAvatarButton(Quotient::User* user, QPushButton* btn)
{
    const auto img = user->avatar(128);
    if (img.isNull()) {
        btn->setText(ProfileDialog::tr("No avatar"));
        btn->setIcon({});
    } else {
        btn->setText({});
        btn->setIcon(QPixmap::fromImage(img));
        btn->setIconSize(img.size());
    }
}

ProfileDialog::ProfileDialog(Quotient::AccountRegistry* accounts,
                             MainWindow* parent)
    : Dialog(tr("User profiles"), parent)
    , m_settings("UI/ProfileDialog")
    , m_avatar(new QPushButton)
    , m_accountSelector(new AccountSelector(accounts))
    , m_displayName(new QLineEdit)
    , m_accessTokenLabel(new QLabel)
    , m_currentAccount(nullptr)
{
    auto* accountLayout = addLayout<QFormLayout>();
    accountLayout->addRow(tr("Account"), m_accountSelector);

    connect(m_accountSelector, &AccountSelector::currentAccountChanged, this,
            &ProfileDialog::load);
    connect(accounts, &Quotient::AccountRegistry::rowsAboutToBeRemoved, this,
            [this, accounts] {
                if (accounts->size() == 1)
                    close(); // The last account is about to be dropped
            });

    auto cardLayout = addLayout<QHBoxLayout>();
    m_avatar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    cardLayout->addWidget(m_avatar, Qt::AlignLeft|Qt::AlignTop);

    connect(m_avatar, &QPushButton::clicked, this, &ProfileDialog::uploadAvatar);

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

    m_deviceTable = new DeviceTable();
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

void ProfileDialog::setAccount(Quotient::Connection* newAccount)
{
    m_accountSelector->setAccount(newAccount);
}

Quotient::Connection* ProfileDialog::account() const
{
    return m_currentAccount;
}

void ProfileDialog::DeviceTable::markupRow(int row, void (QFont::*fontFn)(bool),
                                           const QString& rowToolTip,
                                           bool flagValue)
{
    Q_ASSERT(row < rowCount());
    for (int c = DeviceName; c < columnCount(); ++c)
        if (auto* it = item(row, c)) {
            it->setToolTip(rowToolTip);
            auto font = it->font();
            (font.*fontFn)(flagValue);
            it->setFont(font);
        }
}

void ProfileDialog::DeviceTable::fillPendingData(const QString& currentDeviceId)
{
    setRowCount(2);
    setItem<DeviceId>(0, currentDeviceId);
    setItem<LastTimeSeen>(0, QDateTime::currentDateTime());
    markCurrentRow(0);
    {
        auto* loadingMsg = new QTableWidgetItem(tr("Loading other devices..."));
        loadingMsg->setFlags(Qt::NoItemFlags);
        setItem(1, DeviceName, loadingMsg);
    }
}

void ProfileDialog::DeviceTable::refresh(const QVector<Quotient::Device>& devices,
                                         const QString& currentDeviceId)
{
    clearContents();
    setRowCount(devices.size());

    for (int i = 0; i < devices.size(); ++i) {
        auto device = devices[i];
        setItem<DeviceName>(i, device.displayName);
        setItem<DeviceId>(i, device.deviceId);
        if (device.lastSeenTs)
            setItem<LastTimeSeen>(i, QDateTime::fromMSecsSinceEpoch(
                                         *device.lastSeenTs));
        setItem<LastIpAddr>(i, device.lastSeenIp);
        if (device.deviceId == currentDeviceId)
            markCurrentRow(i);
    }

    setSortingEnabled(true);
}

void ProfileDialog::load()
{
    if (m_currentAccount)
        disconnect(m_currentAccount->user(), nullptr, this, nullptr);
    if (m_devicesJob)
        m_devicesJob->abandon();
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
    connect(user, &User::defaultAvatarChanged, this,
            [this] { updateAvatarButton(account()->user(), m_avatar); });

    m_displayName->setText(user->name());
    m_displayName->setFocus();
    connect(user, &User::defaultNameChanged, this,
            [this, user] { m_displayName->setText(user->name()); });

    auto accessToken = account()->accessToken();
    if (Q_LIKELY(accessToken.size() > 10))
        accessToken.replace(5, accessToken.size() - 10, "...");
    m_accessTokenLabel->setText(accessToken);

    m_deviceTable->setSortingEnabled(false);
    m_deviceTable->fillPendingData(m_currentAccount->deviceId());
    if (!m_settings.contains("device_table_state"))
        m_deviceTable->resizeColumnsToContents();

    m_devicesJob = m_currentAccount->callApi<Quotient::GetDevicesJob>();
    connect(m_devicesJob, &BaseJob::success, m_deviceTable, [this] {
        m_devices = m_devicesJob->devices();
        m_deviceTable->refresh(m_devices, m_currentAccount->deviceId());
        if (m_settings.contains("device_table_state"))
            m_deviceTable->horizontalHeader()->restoreState(
                m_settings.value("device_table_state").toByteArray());
        else
            m_deviceTable->sortByColumn(DeviceTable::LastTimeSeen,
                                        Qt::DescendingOrder);
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

    for (const auto& device: std::as_const(m_devices)) {
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

void ProfileDialog::uploadAvatar()
{
    const auto& dirs =
        QStandardPaths::standardLocations(QStandardPaths::PicturesLocation);
    auto* fDlg = new QFileDialog(this, tr("Set avatar"),
                                 dirs.isEmpty() ? QString() : dirs.back());
    fDlg->setFileMode(QFileDialog::ExistingFile);
    fDlg->setMimeTypeFilters({ "image/jpeg", "image/png",
                               "application/octet-stream" });
    fDlg->open();
    connect(fDlg, &QFileDialog::fileSelected, this,
            [this](const QString& fileName) {
                m_newAvatarPath = fileName;
                if (!m_newAvatarPath.isEmpty()) {
                    auto img =
                        QImage(m_newAvatarPath)
                            .scaled(m_avatar->iconSize(), Qt::KeepAspectRatio);
                    m_avatar->setIcon(QPixmap(m_newAvatarPath));
                }
            });
}
