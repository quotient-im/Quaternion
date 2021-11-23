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

#include "roomdialogs.h"

#include "mainwindow.h"
#include "quaternionroom.h"
#include "accountregistry.h"
#include "accountselector.h"
#include "models/orderbytag.h" // For tagToCaption()

#include <user.h>
#include <connection.h>
#include <csapi/create_room.h>
#include <logging.h>
#include <events/roompowerlevelsevent.h>

#include <QtWidgets/QComboBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QMessageBox>
#include <QtGui/QStandardItemModel>

#include <QtCore/QElapsedTimer>
#include <QtCore/QStringBuilder>

RoomDialogBase::RoomDialogBase(const QString& title,
        const QString& applyButtonText,
        QuaternionRoom* r, QWidget* parent,
        QDialogButtonBox::StandardButtons extraButtons)
    : Dialog(title, parent, StatusLine, applyButtonText, extraButtons)
    , room(r), avatar(new QLabel)
    , roomName(new QLineEdit)
    , aliasServer(new QLabel), alias(new QLineEdit)
    , topic(new QPlainTextEdit)
    , publishRoom(new QCheckBox(tr("Publish room in room directory")))
    , guestCanJoin(new QCheckBox(tr("Allow guest accounts to join the room")))
    , mainFormLayout(addLayout<QFormLayout>())
{
    if (room)
    {
        avatar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        avatar->setPixmap({64, 64});
    }
    topic->setTabChangesFocus(true);
    topic->setSizeAdjustPolicy(
                QAbstractScrollArea::AdjustToContentsOnFirstShow);

    // Layout controls

    {
        if (room)
        {
            auto* topLayout = new QHBoxLayout;
            topLayout->addWidget(avatar);
            {
                essentialsLayout = new QFormLayout;
                essentialsLayout->addRow(tr("Room name"), roomName);
                essentialsLayout->addRow(tr("Primary alias"), alias);
                topLayout->addLayout(essentialsLayout);
            }
            mainFormLayout->addRow(topLayout);
        } else {
            mainFormLayout->addRow(tr("Room name"), roomName);
            auto* aliasLayout = new QHBoxLayout;
            aliasLayout->addWidget(new QLabel("#"));
            aliasLayout->addWidget(alias);
            aliasLayout->addWidget(aliasServer);
            mainFormLayout->addRow(tr("Primary alias"), aliasLayout);
        }
    }
    mainFormLayout->addRow(tr("Topic"), topic);
    if (!room) // TODO: Support this in RoomSettingsDialog as well
    {
        mainFormLayout->addRow(publishRoom);
//        formLayout->addRow(guestCanJoin); // TODO: quotient-im/libQuotient#36
    }
}

QComboBox* RoomDialogBase::addVersionSelector(QLayout* layout)
{
    auto* versionSelector = new QComboBox;
    layout->addWidget(versionSelector);
    {
        auto* specLink =
                new QLabel("<a href='https://matrix.org/docs/spec/#complete-list-of-room-versions'>" +
                           tr("About room versions") + "</a>");
        specLink->setOpenExternalLinks(true);
        layout->addWidget(specLink);
    }
    return versionSelector;
}

void RoomDialogBase::refillVersionSelector(QComboBox* selector,
                                           Connection* account)
{
    selector->clear();
    if (account->loadingCapabilities())
    {
        selector->addItem(
                    tr("(loading)", "Loading room versions from the server"),
                    QString());
        selector->setEnabled(false);
        // FIXME: It should be connectSingleShot
        // but sadly connectSingleShot doesn't work with lambdas yet
        connectUntil(account, &Connection::capabilitiesLoaded, this,
            [this,selector,account] {
                refillVersionSelector(selector, account);
                return true;
            });
        return;
    }
    const auto& versions = account->availableRoomVersions();
    for (const auto& v: versions)
    {
        const bool isDefault = v.id == account->defaultRoomVersion();
        const auto postfix =
                isDefault ? tr("default", "Default room version") :
                v.isStable() ? tr("stable", "Stable room version") :
                v.status;
        selector->addItem(v.id % " (" % postfix % ")", v.id);
        const auto idx = selector->count() - 1;
        if (isDefault)
        {
            auto font = selector->itemData(idx, Qt::FontRole).value<QFont>();
            font.setBold(true);
            selector->setItemData(idx, font, Qt::FontRole);
            selector->setCurrentIndex(idx);
        }
        if (!v.isStable())
            selector->setItemData(idx, QColor(Qt::red), Qt::ForegroundRole);
    }
    selector->setEnabled(true);
}

void RoomDialogBase::addEssentials(QWidget* accountControl,
                                   QLayout* versionBox)
{
    Q_ASSERT(accountControl != nullptr && versionBox != nullptr);
    auto* layout = essentialsLayout ? essentialsLayout : mainFormLayout;
    layout->insertRow(0, tr("Account"), accountControl);
    layout->insertRow(1, tr("Room version"), versionBox);
}

bool RoomDialogBase::checkRoomVersion(QString version, Connection* account)
{
    if (account->stableRoomVersions().contains(version))
        return true;

    return QMessageBox::warning(this, tr("Continue with unstable version?"),
            tr("You are using an UNSTABLE room version (%1)."
               " The server may stop supporting it at any moment."
               " Do you still want to use this version?").arg(version),
            QMessageBox::Yes|QMessageBox::No, QMessageBox::No)
        == QMessageBox::Yes;
}

RoomSettingsDialog::RoomSettingsDialog(QuaternionRoom* room, MainWindow* parent)
    : RoomDialogBase(tr("Room settings: %1").arg(room->displayName()),
                     tr("Update room"), room, parent)
    , account(new QLabel(room->connection()->userId()))
    , version(new QLabel(room->version()))
    , tagsList(new QListWidget)
{
    auto* versionBox = new QGridLayout;
    versionBox->addWidget(version, 0, 0);
    if (room->isUnstable())
        versionBox->addWidget(
            new QLabel(tr("This version is unstable! Consider upgrading.")),
            1, 0);
    if (room->canSwitchVersions())
    {
        auto* changeActionButton =
                new QPushButton(tr("Upgrade", "Upgrade a room version"));
        connect(changeActionButton, &QAbstractButton::clicked, this, [this, room] {
            Dialog chooseVersionDlg(tr("Choose new room version"), this,
                    NoStatusLine, tr("Upgrade", "Upgrade a room version"),
                    NoExtraButtons);
            chooseVersionDlg.addWidget(
                new QLabel(tr("You are about to upgrade %1.\n"
                              "This operation cannot be reverted.")
                           .arg(room->displayName())));
            auto* hBox = chooseVersionDlg.addLayout<QHBoxLayout>();
            auto* versionSelector = addVersionSelector(hBox);
            refillVersionSelector(versionSelector, room->connection());
            if (chooseVersionDlg.exec() == QDialog::Accepted)
            {
                version->setText(versionSelector->currentData().toString());
                apply();
            }
        });
        versionBox->addWidget(changeActionButton, 0, 1, -1, 1);
    }
    addEssentials(account, versionBox);
    connect(room, &QuaternionRoom::avatarChanged, this, [this, room] {
        if (!userChangedAvatar)
            avatar->setPixmap(QPixmap::fromImage(room->avatar(64)));
    });
    avatar->setPixmap(QPixmap::fromImage(room->avatar(64)));
    tagsList->setSizeAdjustPolicy(
                QAbstractScrollArea::AdjustToContentsOnFirstShow);
    tagsList->setUniformItemSizes(true);
    tagsList->setSelectionMode(QAbstractItemView::ExtendedSelection);

    mainFormLayout->addRow(tr("Tags"), tagsList);

    auto* roomIdLabel = new QLabel(room->id());
    roomIdLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    mainFormLayout->addRow(tr("Room identifier"), roomIdLabel);

    connect(room, &QObject::destroyed, this, &QObject::deleteLater);

    // Uncomment to debug room display name calculation code
//    auto* refreshNameButton =
//        buttonBox()->addButton(tr("Refresh name"), QDialogButtonBox::ApplyRole);
//    connect(refreshNameButton, &QPushButton::clicked,
//            room, &QuaternionRoom::refreshDisplayName);
}

void RoomSettingsDialog::load()
{
    const auto* plEvt =
        room->getCurrentState<Quotient::RoomPowerLevelsEvent>();
    const int userPl = plEvt->powerLevelForUser(room->localUser()->id());

    roomName->setText(room->name());
    roomName->setReadOnly(plEvt && plEvt->powerLevelForState("m.room.name") > userPl);
    alias->setText(room->canonicalAlias());
    alias->setReadOnly(plEvt && plEvt->powerLevelForState("m.room.canonical_alias") > userPl);
    topic->setPlainText(room->topic());
    topic->setReadOnly(plEvt && plEvt->powerLevelForState("m.room.topic") > userPl);
    // QPlainTextEdit may change some characters before any editing occurs;
    // so save this already adjusted topic to compare later.
    previousTopic = topic->toPlainText();

    tagsList->clear();
    auto roomTags = room->tagNames();
    for (const auto& tag: room->connection()->tagNames())
    {
        auto* item = new QListWidgetItem(tagToCaption(tag), tagsList);
        item->setData(Qt::UserRole, tag);
        item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsUserCheckable);
        item->setCheckState(
                    roomTags.contains(tag) ? Qt::Checked : Qt::Unchecked);
        item->setToolTip(tag);
        tagsList->addItem(item);
    }
}

bool RoomSettingsDialog::validate()
{
    if (room->version() == version->text()
            || (room->canSwitchVersions()
                && checkRoomVersion(version->text(), room->connection())))
        return true; // The room is the same, or it's allowed to change it

    version->setText(room->version());
    return false; // Cancel applying, stay on the settings dialog
}

void RoomSettingsDialog::apply()
{
    using Quotient::Room;
    if (version->text() != room->version())
    {
        setStatusMessage(tr("Creating the new room version, please wait"));
        connectUntil(room, &Room::upgraded, this,
            [this] (const QString&, Room* newRoom) {
                accept();
                static_cast<MainWindow*>(parent())->selectRoom(newRoom);
                return true;
            });
        connectSingleShot(room, &Room::upgradeFailed,
                          this, &Dialog::applyFailed);
        room->switchVersion(version->text());
        return; // It's either a version upgrade or everything else
    }
    if (roomName->text() != room->name())
        room->setName(roomName->text());
    if (alias->text() != room->canonicalAlias())
        room->setCanonicalAlias(alias->text());
    if (topic->toPlainText() != previousTopic)
        room->setTopic(topic->toPlainText());
    auto tags = room->tags();
    for (int i = 0; i < tagsList->count(); ++i)
    {
        const auto* item = tagsList->item(i);
        const auto tagName = item->data(Qt::UserRole).toString();
        if (item->checkState() == Qt::Checked)
            tags[tagName]; // Just ensure the tag is there, no overwriting
        else
            tags.remove(tagName);
    }
    room->setTags(tags, Room::WithinSameState);
    accept();
}

class NextInvitee : public QComboBox
{
    public:
        using QComboBox::QComboBox;

    private:
        void focusInEvent(QFocusEvent* event) override
        {
            QComboBox::focusInEvent(event);
            static_cast<CreateRoomDialog*>(parent())->updatePushButtons();
        }
        void focusOutEvent(QFocusEvent* event) override
        {
            QComboBox::focusOutEvent(event);
            static_cast<CreateRoomDialog*>(parent())->updatePushButtons();
        }
};

class InviteeList : public QListWidget
{
    public:
        using QListWidget::QListWidget;

    private:
        void keyPressEvent(QKeyEvent* event) override
        {
            if (event->key() ==  Qt::Key_Delete)
                delete takeItem(currentRow());
        }
        void mousePressEvent(QMouseEvent* event) override
        {
            if (event->button() == Qt::MiddleButton)
                delete takeItem(currentRow());
        }
};

CreateRoomDialog::CreateRoomDialog(const AccountRegistry* accounts, QWidget* parent)
    : RoomDialogBase(tr("Create room"), tr("Create room"),
                     nullptr, parent, NoExtraButtons)
    , accountChooser(new AccountSelector(accounts))
    , version(nullptr) // Will be initialized below
    , nextInvitee(new NextInvitee)
    , inviteButton(new QPushButton(tr("Add", "Add a user to the list of invitees")))
    , invitees(new QListWidget)
{
    Q_ASSERT(accounts && !accounts->isEmpty());

    auto* versionBox = new QHBoxLayout;
    version = addVersionSelector(versionBox);
    addEssentials(accountChooser, versionBox);
    connect(accountChooser, &AccountSelector::currentAccountChanged,
            this, &CreateRoomDialog::accountSwitched);
    mainFormLayout->insertRow(0, new QLabel(
            tr("Please fill the fields as desired. None are mandatory")));

    nextInvitee->setEditable(true);
    nextInvitee->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    nextInvitee->setMinimumContentsLength(42);
    auto* completer = new QCompleter(nextInvitee);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    completer->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    nextInvitee->setCompleter(completer);
    connect(nextInvitee, &NextInvitee::currentTextChanged,
            this, &CreateRoomDialog::updatePushButtons);
//    connect(nextInvitee, &NextInvitee::editTextChanged,
//            this, &CreateRoomDialog::updateUserList);
    inviteButton->setFocusPolicy(Qt::NoFocus);
    inviteButton->setDisabled(true);
    connect(inviteButton, &QPushButton::clicked, [this] {
        auto userName = nextInvitee->currentText();
        if (userName.indexOf('@') == -1)
        {
            userName.prepend('@');
            if (userName.indexOf(':') == -1)
                userName += ':' + accountChooser->currentAccount()->domain();
        }
        auto* item = new QListWidgetItem(userName);
        if (nextInvitee->currentIndex() != -1)
            item->setData(Qt::UserRole, nextInvitee->currentData(Qt::UserRole));
        invitees->addItem(item);
        nextInvitee->clear();
    });
    invitees->setSizeAdjustPolicy(
                QAbstractScrollArea::AdjustToContentsOnFirstShow);
    invitees->setUniformItemSizes(true);
    invitees->setSortingEnabled(true);

    // Layout additional controls

    auto* inviteLayout = new QHBoxLayout;
    inviteLayout->addWidget(nextInvitee);
    inviteLayout->addWidget(inviteButton);

    mainFormLayout->addRow(tr("Invite user(s)"), inviteLayout);
    mainFormLayout->addRow("", invitees);

    setPendingApplyMessage(tr("Creating the room, please wait"));

    if (accounts->size() > 1)
        accountChooser->setFocus();
    else
        roomName->setFocus();
}

void CreateRoomDialog::updatePushButtons()
{
    inviteButton->setEnabled(!nextInvitee->currentText().isEmpty());
    if (inviteButton->isEnabled() && nextInvitee->hasFocus())
        inviteButton->setDefault(true);
    else
        buttonBox()->button(QDialogButtonBox::Ok)->setDefault(true);
}

void CreateRoomDialog::load()
{
    qDebug() << "Loading the dialog";
    roomName->clear();
    alias->clear();
    topic->clear();
    previousTopic.clear();
    nextInvitee->clear();
    accountSwitched();
    invitees->clear();
}

bool CreateRoomDialog::validate()
{
    auto* connection = accountChooser->currentAccount();
    if (checkRoomVersion(version->currentData().toString(), connection))
        return true;

    refillVersionSelector(version, connection);
    return false;
}

void CreateRoomDialog::apply()
{
    using namespace Quotient;
    QStringList userIds;
    for (int i = 0; i < invitees->count(); ++i)
        if (auto* user = invitees->item(i)->data(Qt::UserRole).value<User*>())
            userIds.push_back(user->id());
        else
            userIds.push_back(invitees->item(i)->text());

    auto* job = accountChooser->currentAccount()->createRoom(
            publishRoom->isChecked() ?
                Connection::PublishRoom : Connection::UnpublishRoom,
            alias->text(), roomName->text(), topic->toPlainText(),
            userIds, "", version->currentData().toString(), false);

    connect(job, &BaseJob::success, this, &Dialog::accept);
    connect(job, &BaseJob::failure, this, [this,job] {
        applyFailed(job->errorString());
    });
}

void CreateRoomDialog::accountSwitched()
{
    const auto& savedCurrentText = nextInvitee->currentText();

    auto* connection = accountChooser->currentAccount();
    refillVersionSelector(version, connection);
    aliasServer->setText(':' + connection->domain());

    auto* completer = nextInvitee->completer();
    Q_ASSERT(completer != nullptr && connection != nullptr);

    auto*& model = userLists[connection];
    if (!model) {
        model = new QStandardItemModel(completer);

//    auto prefix =
//            savedCurrentText.midRef(savedCurrentText.startsWith('@') ? 1 : 0);
//    if (prefix.size() >= 3)
//    {
        QElapsedTimer et; et.start();
        for (auto* u: connection->users())
        {
            if (!u->isGuest())
            {
                // It would be great to show u->fullName() rather than
                // just u->id(); unfortunately, this implies fetching profiles
                // for the whole list of users known to a given account, which
                // is terribly inefficient
                auto* item = new QStandardItem(u->id());
                item->setData(QVariant::fromValue(u));
                model->appendRow(item);
            }
        }
        qDebug() << "Completion candidates:" << model->rowCount()
                 << "out of" << connection->users().size()
                 << "filled in" << et;
//    }
    }
    nextInvitee->setModel(model);
    nextInvitee->setEditText(savedCurrentText);
    completer->setCompletionPrefix(savedCurrentText);
}
