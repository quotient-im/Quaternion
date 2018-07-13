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

#include "roomlistdock.h"

#include <QtCore/QSettings>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QInputDialog>

#include "models/roomlistmodel.h"
#include "quaternionroom.h"
#include <connection.h>

class RoomListItemDelegate : public QStyledItemDelegate
{
    public:
        explicit RoomListItemDelegate(QObject* parent = nullptr)
            : QStyledItemDelegate(parent)
            , highlightColor(QSettings()
                             .value("UI/highlight_color", QColor("orange"))
                             .value<QColor>())
        { }

        void paint(QPainter *painter, const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

    private:
        QColor highlightColor;
};

void RoomListItemDelegate::paint(QPainter* painter,
         const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem o { option };

    if (index.data(RoomListModel::HasUnreadRole).toBool())
        o.font.setBold(true);

    if (index.data(RoomListModel::HighlightCountRole).toInt() > 0)
    {
        // Highlighting the text may not work out on monochrome colour schemes,
        // hence duplicating with italic font.
        o.palette.setColor(QPalette::Text, highlightColor);
        o.font.setItalic(true);
    }

    using QMatrixClient::JoinState;
    QString joinState = index.data(RoomListModel::JoinStateRole).toString();
    if (joinState == toCString(JoinState::Invite))
        o.font.setItalic(true);
    else if (joinState == toCString(JoinState::Leave))
        o.font.setStrikeOut(true);

    QStyledItemDelegate::paint(painter, o, index);
}

RoomListDock::RoomListDock(QWidget* parent)
    : QDockWidget("Rooms", parent)
    , selectedRoomCache(nullptr)
{
    setObjectName("RoomsDock");
    model      = new RoomListModel(this);
    view       = new QListView();
    proxyModel = new QSortFilterProxyModel();
    proxyModel->setDynamicSortFilter(true);
    proxyModel->setSourceModel(model);
    proxyModel->sort(0);
    view->setModel(proxyModel);
    view->setItemDelegate(new RoomListItemDelegate(this));
    connect( view, &QListView::activated, this, &RoomListDock::rowSelected );
    connect( view, &QListView::clicked, this, &RoomListDock::rowSelected);
    connect( model, &RoomListModel::rowsInserted,
             this, &RoomListDock::refreshTitle );
    connect( model, &RoomListModel::rowsRemoved,
             this, &RoomListDock::refreshTitle );
    connect( model, &RoomListModel::modelAboutToBeReset, this, [this] {
        selectedRoomCache = getSelectedRoom();
    });
    connect( model, &RoomListModel::modelReset, this, [this] {
        view->setCurrentIndex(
            proxyModel->mapFromSource(model->indexOf(selectedRoomCache)));
        selectedRoomCache = nullptr;
        refreshTitle();
    });
    setWidget(view);

    contextMenu = new QMenu(this);
    markAsReadAction = new QAction(tr("Mark room as read"), this);
    connect(markAsReadAction, &QAction::triggered, this, &RoomListDock::menuMarkReadSelected);
    contextMenu->addAction(markAsReadAction);
    contextMenu->addSeparator();
    addTagsAction = new QAction(tr("Add tags..."), this);
    connect(addTagsAction, &QAction::triggered, this, &RoomListDock::addTagsSelected);
    contextMenu->addAction(addTagsAction);
    contextMenu->addSeparator();
    joinAction = new QAction(tr("Join room"), this);
    connect(joinAction, &QAction::triggered, this, &RoomListDock::menuJoinSelected);
    contextMenu->addAction(joinAction);
    leaveAction = new QAction(this);
    connect(leaveAction, &QAction::triggered, this, &RoomListDock::menuLeaveSelected);
    contextMenu->addAction(leaveAction);
    contextMenu->addSeparator();
    forgetAction = new QAction(tr("Forget room"), this);
    connect(forgetAction, &QAction::triggered, this, &RoomListDock::menuForgetSelected);
    contextMenu->addAction(forgetAction);

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &RoomListDock::showContextMenu);
}

void RoomListDock::addConnection(QMatrixClient::Connection* connection)
{
    model->addConnection(connection);
}

void RoomListDock::rowSelected(const QModelIndex& index)
{
    if (index.isValid())
        emit roomSelected( model->roomAt(proxyModel->mapToSource(index)));
}

void RoomListDock::showContextMenu(const QPoint& pos)
{
    QModelIndex index = view->indexAt(view->mapFromParent(pos));
    if( !index.isValid() )
        return;
    auto room = model->roomAt(proxyModel->mapToSource(index));

    using QMatrixClient::JoinState;
    bool joined = room->joinState() == JoinState::Join;
    bool invited = room->joinState() == JoinState::Invite;
    markAsReadAction->setEnabled(joined);
    addTagsAction->setEnabled(joined);
    joinAction->setEnabled(!joined);
    leaveAction->setText(invited ? tr("Reject invitation") : tr("Leave room"));
    leaveAction->setEnabled(room->joinState() != JoinState::Leave);
    forgetAction->setVisible(!invited);

    contextMenu->popup(mapToGlobal(pos));
}

QuaternionRoom* RoomListDock::getSelectedRoom() const
{
    QModelIndex index = view->currentIndex();
    return !index.isValid() ? nullptr : model->roomAt(proxyModel->mapToSource(index));
}

void RoomListDock::menuJoinSelected()
{
    // The user has been invited to the room
    if (auto room = getSelectedRoom())
    {
        Q_ASSERT(room->connection());
        room->connection()->joinRoom(room->id());
    }
}

void RoomListDock::menuLeaveSelected()
{
    if (auto room = getSelectedRoom())
        room->leaveRoom();
}

void RoomListDock::menuForgetSelected()
{
    if (auto room = getSelectedRoom())
        room->connection()->forgetRoom(room->id());
}

void RoomListDock::menuMarkReadSelected()
{
    if (auto room = getSelectedRoom())
        room->markAllMessagesAsRead();
}

void RoomListDock::addTagsSelected()
{
    if (auto room = getSelectedRoom())
    {
        auto tagsInput = QInputDialog::getMultiLineText(this,
                tr("Enter new tags for the room"),
                tr("Enter tags to add to this room, one tag per line"));
        if (tagsInput.isEmpty())
            return;

        auto tags = room->tags();
        for (const auto& tag: tagsInput.split('\n'))
        {
            // No overwriting, just ensure the tag exists
            tags[tag == tr("Favourite") ? QMatrixClient::FavouriteTag :
                 tag == tr("Low priority") ? QMatrixClient::LowPriorityTag :
                 tag];
        }
        room->setTags(tags);
    }
}

void RoomListDock::refreshTitle()
{
    setWindowTitle(tr("Rooms (%1)").arg(model->rowCount(QModelIndex())));
}
