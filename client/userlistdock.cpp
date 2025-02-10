/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2015 Felix Rohrbach <kde@fxrh.de>              *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#include "userlistdock.h"

#include <QtWidgets/QTableView>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMenu>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QInputDialog>
#include <QtGui/QGuiApplication>

#include <Quotient/connection.h>
#include <Quotient/events/roompowerlevelsevent.h>
#include <Quotient/room.h>
#include <Quotient/user.h>

#include "models/userlistmodel.h"
#include "quaternionroom.h"

UserListDock::UserListDock(QWidget* parent)
    : QDockWidget(tr("Users"), parent)
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
    // Derive the member icon size from that of the default icon used when
    // the member doesn't have an avatar
    const auto iconExtent = m_view->fontMetrics().height() * 3 / 2;
    m_view->setIconSize(
        QIcon::fromTheme("user-available", QIcon(":/irc-channel-joined"))
            .actualSize({ iconExtent, iconExtent }));
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
        if (QGuiApplication::mouseButtons() & Qt::MiddleButton)
            startChatSelected();
    });

    m_model = new UserListModel(m_view);
    m_view->setModel(m_model);

    connect( m_model, &UserListModel::membersChanged,
             this, &UserListDock::refreshTitle );
    connect( m_model, &QAbstractListModel::modelReset,
             this, &UserListDock::refreshTitle );
    connect(m_filterline, &QLineEdit::textEdited,
             m_model, &UserListModel::filter);

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
    if (getSelectedUser().isEmpty())
        return;

    auto* contextMenu = new QMenu(this);

    contextMenu->addAction(QIcon::fromTheme("contact-new"),
        tr("Open direct chat"), this, &UserListDock::startChatSelected);
    contextMenu->addAction(tr("Mention user"), this,
        &UserListDock::requestUserMention);
    QAction* ignoreAction =
        contextMenu->addAction(QIcon::fromTheme("mail-thread-ignored"),
            tr("Ignore user"), this, &UserListDock::ignoreUser);
    ignoreAction->setCheckable(true);
    contextMenu->addSeparator();

    const auto* plEvt =
        m_currentRoom->currentState().get<Quotient::RoomPowerLevelsEvent>();
    const int userPl =
        plEvt ? plEvt->powerLevelForUser(m_currentRoom->localMember().id()) : 0;

    if (!plEvt || userPl >= plEvt->kick()) {
        contextMenu->addAction(QIcon::fromTheme("im-ban-kick-user"),
            tr("Kick user"), this,&UserListDock::kickUser);
    }
    if (!plEvt || userPl >= plEvt->ban()) {
        contextMenu->addAction(QIcon::fromTheme("im-ban-user"),
            tr("Ban user"), this, &UserListDock::banUser);
    }

    contextMenu->popup(mapToGlobal(pos));
    ignoreAction->setChecked(isIgnored());
}

void UserListDock::startChatSelected()
{
    if (auto userId = getSelectedUser(); !userId.isEmpty())
        m_currentRoom->connection()->requestDirectChat(userId);
}

void UserListDock::requestUserMention()
{
    if (auto userId = getSelectedUser(); !userId.isEmpty())
        emit userMentionRequested(userId);
}

void UserListDock::kickUser()
{
    if (auto userId = getSelectedUser(); !userId.isEmpty())
    {
        bool ok;
        const auto reason = QInputDialog::getText(this,
                tr("Kick %1").arg(userId), tr("Reason"),
                QLineEdit::Normal, nullptr, &ok);
        if (ok) {
            m_currentRoom->kickMember(userId, reason);
        }
    }
}

void UserListDock::banUser()
{
    if (auto userId = getSelectedUser(); !userId.isEmpty())
    {
        bool ok;
        const auto reason = QInputDialog::getText(this,
                tr("Ban %1").arg(userId), tr("Reason"),
                QLineEdit::Normal, nullptr, &ok);
        if (ok) {
            m_currentRoom->ban(userId, reason);
        }
    }
}

void UserListDock::ignoreUser()
{
    if (auto* user = m_currentRoom->connection()->user(getSelectedUser())) {
        if (!user->isIgnored())
            user->ignore();
        else
            user->unmarkIgnore();
    }
}

bool UserListDock::isIgnored()
{
    if (auto memberId = getSelectedUser(); !memberId.isEmpty())
        return m_currentRoom->connection()->isIgnored(memberId);
    return false;
}

QString UserListDock::getSelectedUser() const
{
    auto index = m_view->currentIndex();
    if (!index.isValid())
        return {};
    const auto member = m_model->userAt(index);
    Q_ASSERT(!member.isEmpty());
    return member.id();
}
