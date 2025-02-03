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
#include "logging_categories.h"
#include "verificationdialog.h"

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
#include <QtWidgets/QToolButton>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>

#include <QtCore/QStandardPaths>

using Quotient::BaseJob, Quotient::User, Quotient::Room;
using namespace Qt::StringLiterals;

namespace {
// TODO: move to libQuotient
//! Like std::clamp but admits a different (usually larger) type for the value
template <typename T>
inline constexpr T clamp(const auto& v, const T& lo = std::numeric_limits<T>::min(),
                         const T& hi = std::numeric_limits<T>::max())
{
    return v < lo ? lo : hi < v ? hi : static_cast<T>(v);
}
}

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
                   : data(Qt::UserRole).toDateTime() < other.data(Qt::UserRole).toDateTime();
    }
};

/*! Device table class
 *
 * Encapsulates the columns model and formatting
 */
class ProfileDialog::DeviceTable : public QTableWidget {
public:
    enum Columns : int {
        Verified = 0,
        DeviceName,
        DeviceId,
        LastTimeSeen,
        LastIpAddr,
        ColumnsCount // Only for size validation; do not use for real columns!
    };
    DeviceTable();
    ~DeviceTable() override = default;

    template <Columns ColumnN>
    using ItemType = std::conditional_t<ColumnN == LastTimeSeen,
                                        TimestampTableItem, QTableWidgetItem>;

    template <Columns ColumnN>
    static constexpr auto itemAlignment =
        ColumnN == Verified ? Qt::AlignCenter : (Qt::AlignLeft | Qt::AlignVCenter);

    template <Columns ColumnN>
    static constexpr auto itemFlags =
        Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled
        | Qt::ItemFlag((ColumnN == DeviceName) & Qt::ItemIsEditable);

    template <Columns ColumnN, typename... DataT>
        requires std::is_constructible_v<ItemType<ColumnN>, DataT...>
    auto emplaceItem(auto row, const DataT&... data)
    {
        auto* item = new ItemType<ColumnN>(data...);
        item->setTextAlignment(itemAlignment<ColumnN>);
        item->setFlags(itemFlags<ColumnN>);
        QTableWidget::setItem(clamp<int>(row, 0), ColumnN, item);
        return item;
    }

    void markupRow(int row, void (QFont::*fontFn)(bool), bool flagValue = true);
    void markCurrentDevice(int row) { markupRow(row, &QFont::setBold); }

    void fillPendingData(const QString& currentDeviceId);
    void refresh(const QVector<Quotient::Device>& devices, ProfileDialog* profileDialog);
};

ProfileDialog::DeviceTable::DeviceTable()
{
    // Must be synchronised with DeviceTable::Columns
    static const QStringList Headers{
        {}, tr("Device display name"), tr("Device ID"), tr("Last time seen"), tr("Last IP address")
    };
    QUO_CHECK(Headers.size() == ColumnsCount);

    setColumnCount(ColumnsCount);
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
    const auto img = user->avatar(128, [] {});
    if (img.isNull()) {
        btn->setText(ProfileDialog::tr("No avatar"));
        btn->setIcon({});
    } else {
        btn->setText({});
        btn->setIcon(QPixmap::fromImage(img));
        btn->setIconSize(img.size());
    }
}

ProfileDialog::ProfileDialog(Quotient::AccountRegistry* accounts, MainWindow* parent)
    : Dialog(tr("User profiles"), QDialogButtonBox::Reset | QDialogButtonBox::Close, parent,
             Dialog::StatusLine)
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

    // TODO: connect the title change to any changes in the dialog data
    // button(QDialogButtonBox::Close)->setText(tr("Apply and close"));

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

void ProfileDialog::setVerifiedItem(int row, const QString& deviceId)
{
    if (m_currentAccount->deviceId() == deviceId)
        m_deviceTable->emplaceItem<DeviceTable::Verified>(row, tr("This device"));
    else if (m_currentAccount->isVerifiedDevice(m_currentAccount->userId(), deviceId)) {
        m_deviceTable->emplaceItem<DeviceTable::Verified>(row, QIcon::fromTheme(u"security-high"_s),
                                                          tr("Verified"));
    } else {
        auto* verifyAction =
            new QAction(QIcon::fromTheme(u"security-medium"_s), tr("Verify..."), this);
        using KVSession = Quotient::KeyVerificationSession;
        connect(verifyAction, &QAction::triggered, this, [this, deviceId, verifyAction] {
            if (auto session = verifyAction->data().value<KVSession*>()) {
                if (session->state() != KVSession::CANCELED)
                    session->cancelVerification(KVSession::USER);
            } else
                initiateVerification(deviceId, verifyAction);
        });

        auto* verifyButton = new QToolButton();
        verifyButton->setToolButtonStyle(Qt::ToolButtonFollowStyle);
        verifyButton->setAutoRaise(true);
        verifyButton->setDefaultAction(verifyAction);
        verifyButton->setEnabled(m_currentAccount->encryptionEnabled());
        m_deviceTable->setCellWidget(clamp<int>(row, 0), DeviceTable::Verified, verifyButton);
    }
}

void ProfileDialog::refreshDevices()
{
    m_devicesJob = m_currentAccount->callApi<Quotient::GetDevicesJob>();
    connect(m_devicesJob, &BaseJob::success, m_deviceTable, [this] {
        m_devices = m_devicesJob->devices();
        m_deviceTable->refresh(m_devices, this);
        if (m_settings.contains("device_table_state"))
            m_deviceTable->horizontalHeader()->restoreState(
                m_settings.value("device_table_state").toByteArray());
        else
            m_deviceTable->sortByColumn(DeviceTable::LastTimeSeen,
                                        Qt::DescendingOrder);
    });
}

void ProfileDialog::DeviceTable::markupRow(int row, void (QFont::*fontFn)(bool), bool flagValue)
{
    Q_ASSERT(row < rowCount());
    for (int c = 0; c < columnCount(); ++c)
        if (auto* it = item(row, c)) {
            auto font = it->font();
            (font.*fontFn)(flagValue);
            it->setFont(font);
        }
}

void ProfileDialog::DeviceTable::fillPendingData(const QString& currentDeviceId)
{
    setRowCount(2);
    emplaceItem<DeviceId>(0, currentDeviceId);
    emplaceItem<LastTimeSeen>(0, QDateTime::currentDateTime());
    markCurrentDevice(0);
    {
        emplaceItem<DeviceName>(1, tr("Loading other devices..."))->setFlags(Qt::NoItemFlags);
        markupRow(1, &QFont::setItalic);
    }
}

void ProfileDialog::DeviceTable::refresh(const QVector<Quotient::Device>& devices,
                                         ProfileDialog* profileDialog)
{
    if (!std::in_range<int>(devices.size()))
        qCCritical(MAIN) << "The number of devices on the account is out of bounds, only the first"
                         << std::numeric_limits<int>::max() << "devices will be shown";
    clearContents();
    setRowCount(clamp<int>(devices.size(), 0));

    const auto* currentAccount = profileDialog->account();
    for (int i = 0; i < rowCount(); ++i) {
        const auto& device = devices[i];
        profileDialog->setVerifiedItem(i, device.deviceId);
        emplaceItem<DeviceName>(i, device.displayName);
        emplaceItem<DeviceId>(i, device.deviceId);
        if (device.lastSeenTs)
            emplaceItem<LastTimeSeen>(i, QDateTime::fromMSecsSinceEpoch(*device.lastSeenTs));
        emplaceItem<LastIpAddr>(i, device.lastSeenIp);
        if (device.deviceId == currentAccount->deviceId())
            markCurrentDevice(i);
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
            [this, user] { updateAvatarButton(user, m_avatar); });

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

    refreshDevices();
}

void ProfileDialog::apply()
{
    if (!m_currentAccount) {
        qCWarning(MAIN)
            << "ProfileDialog: no account chosen, can't apply changes";
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

inline QString errorToMessage(Quotient::KeyVerificationSession::Error e)
{
    switch (e) {
        using enum Quotient::KeyVerificationSession::Error;
    case TIMEOUT:
    case REMOTE_TIMEOUT: return ProfileDialog::tr("Verification timed out");
    case USER:           return ProfileDialog::tr("Verification was cancelled");
    case REMOTE_USER:    return ProfileDialog::tr("Verification was cancelled on the other device");
    case MISMATCHED_SAS:
    case REMOTE_MISMATCHED_SAS:
        return ProfileDialog::tr("Verification failed: emojis did not match");
    default:             return ProfileDialog::tr("Verification did not succeed");
    }
}

Quotient::KeyVerificationSession* ProfileDialog::initiateVerification(const QString& deviceId,
                                                                      QAction* verifyAction)
{
    using namespace Quotient;
    auto* session = account()->startKeyVerificationSession(account()->userId(), deviceId);
    verifyAction->setData(QVariant::fromValue(session));
    verifyAction->setText(tr("Cancel"));
    setStatusMessage(tr("Please accept the verification request on the device you want to verify"));
    connect(session, &KeyVerificationSession::finished, this, [this, session, verifyAction] {
        if (session->state() == KeyVerificationSession::DONE)
            refreshDevices();
        else {
            setStatusMessage(errorToMessage(session->error()));
            verifyAction->setText(tr("Verify..."));
            verifyAction->setData(QVariant::fromValue(nullptr));
        }
    });
    // TODO: when the library supports other methods, ask to choose instead of opting
    //       for SAS straight away
    QtFuture::connect(session, &KeyVerificationSession::stateChanged).then([this, session] {
        using enum KeyVerificationSession::State;
        if (auto s = session->state(); s == READY) {
            setStatusMessage({});
            session->sendStartSas();
        } else if (s != WAITINGFORACCEPT && s != ACCEPTED && s != CANCELED && s != DONE) {
            qCritical(MAIN) << "Unexpected state of key verification session:" << terse << s;
            session->cancelVerification(KeyVerificationSession::UNEXPECTED_MESSAGE);
        }
    });
    QtFuture::connect(session, &KeyVerificationSession::sasEmojisChanged)
      .then([this, session] {
          QUO_ALARM_X(session->sasEmojis().empty(),
                      "Empty SAS emoji sequence, the session seems to be broken");
          auto dialog = new VerificationDialog(session, this);
          dialog->setModal(true);
          dialog->setAttribute(Qt::WA_DeleteOnClose);
          dialog->show();
      });
    connect(this, &QDialog::finished, session,
            [session] { session->cancelVerification(KeyVerificationSession::USER); });
    return session;
}
