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

#include "roomsettingsdialog.h"

#include "quaternionroom.h"
#include "lib/connection.h"
#include "lib/jobs/generated/create_room.h"

#include <QtWidgets/QComboBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QFormLayout>

class NextInvitee : public QComboBox
{
    public:
        using QComboBox::QComboBox;

    private:
        void focusOutEvent(QFocusEvent*) override
        {
            static_cast<RoomSettingsDialog*>(parent())->updatePushButtons();
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
            if (event->button() == Qt::MidButton)
                delete takeItem(currentRow());
        }
};

RoomSettingsDialog::RoomSettingsDialog(const connections_t& connections,
                                       QWidget* parent)
    : RoomSettingsDialog(tr("Create room"), tr("Create room"),
                         nullptr, parent, connections, Dialog::NoExtraButtons)
{ }

RoomSettingsDialog::RoomSettingsDialog(QuaternionRoom* room, QWidget* parent)
    : RoomSettingsDialog(tr("Room settings"), tr("Update room"), room, parent)
{ }

RoomSettingsDialog::RoomSettingsDialog(const QString& title,
        const QString& applyButtonText,
        QuaternionRoom* r, QWidget* parent, const connections_t& cs,
        QDialogButtonBox::StandardButtons extraButtons)
    : Dialog(title, parent, Dialog::LongApply, applyButtonText, extraButtons)
    , connections(cs), room(r), avatar(new QLabel)
    , roomName(new QLineEdit), alias(new QLineEdit), topic(new QPlainTextEdit)
    , publishRoom(new QCheckBox(tr("Publish room in room directory")))
    , guestCanJoin(new QCheckBox(tr("Allow guest accounts to join the room")))
{
    if (!room)
    {
        account = new QComboBox;
        nextInvitee = new NextInvitee;
        inviteButton = new QPushButton(tr("Invite"));
        invitees = new QListWidget;
    }
    avatar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    avatar->setPixmap({64, 64});
    topic->setSizeAdjustPolicy(
                QAbstractScrollArea::AdjustToContentsOnFirstShow);
    nextInvitee->setEditable(true);
    connect(nextInvitee, &QComboBox::currentTextChanged,
            this, &RoomSettingsDialog::updatePushButtons);
    inviteButton->setFocusPolicy(Qt::NoFocus);
    inviteButton->setDisabled(true);
    connect(inviteButton, &QPushButton::clicked, [this] {
        invitees->addItem(nextInvitee->currentText());
        nextInvitee->clear();
    });
    invitees->setSizeAdjustPolicy(
                QAbstractScrollArea::AdjustToContentsOnFirstShow);
    invitees->setUniformItemSizes(true);
    invitees->setSortingEnabled(true);

    // Layout controls

    auto* formLayout = addLayout<QFormLayout>();
    if (!room)
        formLayout->addRow(new QLabel(
                tr("Please fill the fields as desired. None are mandatory")));
    {
        auto* topLayout = new QHBoxLayout;
        topLayout->addWidget(avatar);
        {
            auto* topFormLayout = new QFormLayout;
            if (connections.size() > 1)
                topFormLayout->addRow(tr("Account"), account);
            topFormLayout->addRow(tr("Room name"), roomName);
            topFormLayout->addRow(tr("Primary alias"), alias);
            topLayout->addLayout(topFormLayout);
        }
        formLayout->addRow(topLayout);
    }
    formLayout->addRow(tr("Topic"), topic);
    formLayout->addRow(publishRoom);
    formLayout->addRow(guestCanJoin);

    auto* inviteLayout = new QHBoxLayout;
    inviteLayout->addWidget(nextInvitee);
    inviteLayout->addWidget(inviteButton);

    formLayout->addRow(tr("Invite user(s)"), inviteLayout);
    formLayout->addRow("", invitees);

    if (connections.size() > 1)
        account->setFocus();
    else
        roomName->setFocus();
}

void RoomSettingsDialog::load()
{
    if (room)
    {
        roomName->setText(room->name());
        alias->setText(room->canonicalAlias());
        topic->setPlainText(room->topic());
        nextInvitee->clear();
        invitees->clear();
    } else {
        account->clear();
        for (auto* c: connections)
            account->addItem(c->userId(), QVariant::fromValue(c));
    }
}

void RoomSettingsDialog::apply()
{
    using namespace QMatrixClient;
    BaseJob* job = nullptr;
    if (!room)
    {
        auto* connection =
                account->currentData(Qt::UserRole).value<Connection*>();
        QVector<QString> userIds;
        for (int i = 0; i < invitees->count(); ++i)
            userIds.push_back(invitees->item(i)->text());
        job = connection->createRoom(
                publishRoom->isChecked() ?
                    Connection::PublishRoom : Connection::UnpublishRoom,
                alias->text(), roomName->text(), topic->toPlainText(),
                userIds, "", false, guestCanJoin->isChecked());
    } else {
        // TODO: Construct needed requests and send them to the server
    }
    connect(job, &BaseJob::success, this, &Dialog::accept);
    connect(job, &BaseJob::failure, this, [this,job] {
        applyFailed(job->errorString());
    });
}

void RoomSettingsDialog::updatePushButtons()
{
    inviteButton->setEnabled(!nextInvitee->currentText().isEmpty());
    if (inviteButton->isEnabled() && nextInvitee->hasFocus())
        inviteButton->setDefault(true);
    else
        buttonBox()->button(QDialogButtonBox::Ok)->setDefault(true);
}
