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
#include <QtGui/QGuiApplication>

#include <connection.h>
#include <room.h>
#include <user.h>
#include "models/userlistmodel.h"
#include "quaternionroom.h"

UserListDock::UserListDock(QWidget* parent)
    : QDockWidget(tr("Users"), parent)
    , contextMenu(new QMenu(this))
{
    setObjectName(QStringLiteral("UsersDock"));

    m_box = new QVBoxLayout();

    m_box->addSpacing(1);
    m_filterline = new QLineEdit(this);
    m_filterline->setPlaceholderText(tr("Search"));
    m_filterline->setDisabled(true);
    m_box->addWidget(m_filterline);

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
    connect( m_view, &QTableView::pressed, this, [this] {
        if (QGuiApplication::mouseButtons() & Qt::MidButton)
            startChatSelected();
    });

    m_model = new UserListModel();
    m_view->setModel(m_model);

    connect( m_model, &UserListModel::membersChanged,
             this, &UserListDock::refreshTitle );
    connect( m_model, &QAbstractListModel::modelReset,
             this, &UserListDock::refreshTitle );
    connect(m_filterline, &QLineEdit::textEdited,
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

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested,
            this, &UserListDock::showContextMenu);
}

void UserListDock::setRoom(QuaternionRoom* room)
{
    if (m_currentRoom)
        m_currentRoom->setCachedUserFilter(m_filterline->text());
    m_currentRoom = room;
    m_model->setRoom(room);
    m_filterline->setEnabled(room);
    m_filterline->setText(room ? room->cachedUserFilter() : "");
    m_model->filter(m_filterline->text());
}

void UserListDock::refreshTitle()
{
    setWindowTitle(tr("Users") +
        (!m_currentRoom ? QString() :
         ' ' + (m_model->rowCount() == m_currentRoom->joinedCount() ?
                    QStringLiteral("(%L1)").arg(m_currentRoom->joinedCount()) :
                    tr("(%L1 out of %L2)", "%found out of %total users")
                    .arg(m_model->rowCount()).arg(m_currentRoom->joinedCount())))
    );
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
    if (auto* user = getSelectedUser())
    {
        bool ok;
        const auto reason = QInputDialog::getText(this,
                tr("Kick %1").arg(user->id()), tr("Reason"),
                QLineEdit::Normal, nullptr, &ok);
        if (ok) {
            m_currentRoom->kickMember(user->id(), reason);
        }
    }
}

void UserListDock::banUser()
{
    if (auto* user = getSelectedUser())
    {
        bool ok;
        const auto reason = QInputDialog::getText(this,
                tr("Ban %1").arg(user->id()), tr("Reason"),
                QLineEdit::Normal, nullptr, &ok);
        if (ok) {
            m_currentRoom->ban(user->id(), reason);
        }
    }
}

void UserListDock::ignoreUser()
{
    if (auto* user = getSelectedUser()) {
        if (!user->isIgnored())
            user->ignore();
        else
            user->unmarkIgnore();
    }
}

bool UserListDock::isIgnored()
{
    if (auto* user = getSelectedUser())
        return user->isIgnored();
    return false;
}

Quotient::User* UserListDock::getSelectedUser() const
{
    auto index = m_view->currentIndex();
    if (!index.isValid())
        return nullptr;
    auto* const user = m_model->userAt(index);
    Q_ASSERT(user);
    return user;
}
