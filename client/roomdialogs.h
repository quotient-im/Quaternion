/*
 * SPDX-FileCopyrightText: 2017 Kitsune Ral <kitsune-ral@users.sf.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "dialog.h"

#include <QtCore/QHash>

namespace Quotient {
class AccountRegistry;
class Connection;
}

class MainWindow;
class QuaternionRoom;
class AccountSelector;

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QCheckBox;
class QPushButton;
class QListWidget;
class QFormLayout;
class QStandardItemModel;

class RoomDialogBase : public Dialog
{
        Q_OBJECT
    protected:
        using Connection = Quotient::Connection;

        RoomDialogBase(const QString& title, const QString& applyButtonText,
            QuaternionRoom* r, QWidget* parent,
            QDialogButtonBox::StandardButtons extraButtons = QDialogButtonBox::Reset);

    protected:
        QuaternionRoom* room;

        QLabel* avatar;
        QLineEdit* roomName;
        QLabel* aliasServer;
        QLineEdit* alias;
        QPlainTextEdit* topic;
        QString previousTopic;
        QCheckBox* publishRoom;
        QCheckBox* guestCanJoin;
        QFormLayout* mainFormLayout;
        QFormLayout* essentialsLayout = nullptr;

        QComboBox* addVersionSelector(QLayout* layout);
        void refillVersionSelector(QComboBox* selector, Connection* account);
        void addEssentials(QWidget* accountControl, QLayout* versionBox);
        bool checkRoomVersion(QString version, Connection* account);
};

class RoomSettingsDialog : public RoomDialogBase
{
        Q_OBJECT
    public:
        RoomSettingsDialog(QuaternionRoom* room, MainWindow* parent = nullptr);

    private slots:
        void load() override;
        bool validate() override;
        void apply() override;

    private:
        QLabel* account;
        QLabel* version;
        QListWidget* tagsList;
        bool userChangedAvatar = false;
};

class CreateRoomDialog : public RoomDialogBase
{
        Q_OBJECT
    public:
        CreateRoomDialog(Quotient::AccountRegistry* accounts,
                         QWidget* parent = nullptr);

    public slots:
        void updatePushButtons();

    private slots:
        void load() override;
        bool validate() override;
        void apply() override;
        void accountSwitched();

    private:
        AccountSelector* accountChooser;
        QComboBox* version;
        QComboBox* nextInvitee;
        QPushButton* inviteButton;
        QListWidget* invitees;

        QHash<Connection*, QStandardItemModel*> userLists;
};
