/**************************************************************************
 *                                                                        *
 * Copyright (C) 2015 Felix Rohrbach <kde@fxrh.de>                        *
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

#include "userlistdock.h"

#include <QtWidgets/QTableView>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMenu>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QInputDialog>

#include <connection.h>
#include <room.h>
#include <user.h>
#include "models/userlistmodel.h"

UserListDock::UserListDock(QWidget* parent)
    : QDockWidget(tr("Users"), parent)
    , contextMenu(new QMenu(this))
{
    setObjectName(QStringLiteral("UsersDock"));

    m_box = new QVBoxLayout();

    m_box->addSpacing(1);
    auto filterline = new QLineEdit(this);
    filterline->setPlaceholderText(tr("Search"));
    m_box->addWidget(filterline);

    m_view = new QTableView(this);
    m_view->setShowGrid(false);
    m_view->horizontalHeader()->setStretchLastSection(true);
    m_view->horizontalHeader()->setVisible(false);
    m_view->verticalHeader()->setVisible(false);
    m_box->addWidget(m_view);

    m_widget = new QWidget(this);
    m_widget->setLayout(m_box);
    setWidget(m_widget);

    connect(m_view, &QTableView::activated,
            this, &UserListDock::requestUserMention);

    m_model = new UserListModel();
    m_view->setModel(m_model);

    connect( m_model, &QAbstractListModel::rowsInserted,
             this, &UserListDock::refreshTitle );
    connect( m_model, &QAbstractListModel::rowsRemoved,
             this, &UserListDock::refreshTitle );
    connect( m_model, &QAbstractListModel::modelReset,
             this, &UserListDock::refreshTitle );
    connect(filterline, &QLineEdit::textEdited,
             m_model, &UserListModel::filter);

    contextMenu->addAction(QIcon::fromTheme("contact-new"),
        tr("Open direct chat"), this, &UserListDock::startChatSelected);
    contextMenu->addAction(tr("Mention user"), this,
        &UserListDock::requestUserMention);
    ignoreAction =
        contextMenu->addAction(QIcon::fromTheme("mail-thread-ignored"),
            tr("Ignore user"), this, &UserListDock::ignoreUser);
    ignoreAction->setCheckable(true);
    contextMenu->addSeparator();
    contextMenu->addAction(QIcon::fromTheme("im-ban-kick-user"),
        tr("Kick user"), this,&UserListDock::kickUser);
    contextMenu->addAction(QIcon::fromTheme("im-ban-user"),
        tr("Ban user"), this, &UserListDock::banUser);
    contextMenu->addAction(tr("Unban user"), this,
        &UserListDock::unbanUser);

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested,
            this, &UserListDock::showContextMenu);
}

void UserListDock::setRoom(QMatrixClient::Room* room)
{
    m_model->setRoom(room);
}

void UserListDock::refreshTitle()
{
    setWindowTitle(tr("Users (%1)").arg(m_model->rowCount(QModelIndex())));
}

void UserListDock::showContextMenu(QPoint pos)
{
    contextMenu->popup(mapToGlobal(pos));
    ignoreAction->setChecked(isIgnored());
}

void UserListDock::startChatSelected()
{
    if (auto* user = getSelectedUser())
        user->requestDirectChat();
}

void UserListDock::requestUserMention()
{
    if (auto* user = getSelectedUser())
        emit userMentionRequested(user);
}

void UserListDock::kickUser()
{
    bool ok;
    const auto kickDialog = QInputDialog::getText(this, tr("Kick User"),
        tr("Reason"), QLineEdit::Normal, nullptr, &ok);
    if (ok) {
        if (auto* user = getSelectedUser())
            m_model->kickUser(user->id(), kickDialog);
    }
}

void UserListDock::banUser()
{
    bool ok;
    const auto banDialog = QInputDialog::getText(this, tr("Ban User"),
        tr("Reason"), QLineEdit::Normal, nullptr, &ok);
    if (ok) {
        if (auto* user = getSelectedUser())
            m_model->banUser(user->id(), banDialog);
    }
}

void UserListDock::unbanUser()
{
    if (auto* user = getSelectedUser())
        m_model->unbanUser(user->id());
}

void UserListDock::ignoreUser()
{
    if (auto* user = getSelectedUser()) {
        if (!user->connection()->isIgnored(user))
            user->ignore();
        else
            user->unmarkIgnore();
    }
}

bool UserListDock::isIgnored()
{
    if (auto* user = getSelectedUser())
        return user->connection()->isIgnored(user);
    return false;
}

QMatrixClient::User* UserListDock::getSelectedUser() const
{
    auto index = m_view->currentIndex();
    if (!index.isValid())
        return nullptr;
    auto* const user = m_model->userAt(index);
    Q_ASSERT(user);
    return user;
}
