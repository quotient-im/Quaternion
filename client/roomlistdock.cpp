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

#include <QtWidgets/QMenu>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QInputDialog>

#include "models/roomlistmodel.h"
#include "quaternionroom.h"
#include <connection.h>
#include <settings.h>

using QMatrixClient::SettingsGroup;

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

    if (!index.parent().isValid())
    {
        o.displayAlignment = Qt::AlignHCenter;
        o.features = QStyleOptionViewItem::Alternate;
    }
    if (!index.parent().isValid() ||
            index.data(RoomListModel::HasUnreadRole).toBool())
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
    view       = new QTreeView();
//    proxyModel = new QSortFilterProxyModel();
//    proxyModel->setDynamicSortFilter(true);
//    proxyModel->setSourceModel(model);
    updateSortingMode();
    view->setModel(model);
    view->setItemDelegate(new RoomListItemDelegate(this));
    view->setAnimated(true);
    view->setUniformRowHeights(true);
    view->setSelectionBehavior(QTreeView::SelectRows);
    view->setHeaderHidden(true);
    view->setIndentation(0);
    view->setRootIsDecorated(false);

    static const auto Expanded = QStringLiteral("expand");
    static const auto Collapsed = QStringLiteral("collapse");
//    connect( view, &QTreeView::activated, this, &RoomListDock::rowSelected );
    connect( view, &QTreeView::clicked, this, &RoomListDock::rowSelected);
    connect( view, &QTreeView::expanded, this, [this] (QModelIndex i) {
        SettingsGroup("UI/RoomsDock")
        .setValue(model->roomGroupAt(i).toString(), Expanded);
    });
    connect( view, &QTreeView::collapsed, this, [this] (QModelIndex i) {
        SettingsGroup("UI/RoomsDock")
        .setValue(model->roomGroupAt(i).toString(), Collapsed);
    });
    connect( model, &RoomListModel::rowsInserted,
             this, &RoomListDock::refreshTitle );
    connect( model, &RoomListModel::rowsRemoved,
             this, &RoomListDock::refreshTitle );
    connect( model, &RoomListModel::modelAboutToBeReset, this, [this] {
        selectedGroupCache = getSelectedGroup();
        selectedRoomCache = getSelectedRoom();
    });
    connect( model, &RoomListModel::modelReset, this, [this] {
        const auto& idx =
            model->indexOf(selectedGroupCache, selectedRoomCache);
//            proxyModel->mapFromSource(model->indexOf(selectedRoomCache));
        view->setCurrentIndex(idx);
        selectedGroupCache.clear();
        selectedRoomCache = nullptr;
        refreshTitle();
        SettingsGroup sg("UI/RoomsDock");
        for (int row = 0; row < model->rowCount({}); ++row)
        {
            const auto& i = model->index(row, 0);
            const auto groupKey = model->roomGroupAt(i).toString();
            const auto expanded = Expanded ==
                    sg.get(groupKey, groupKey == QMatrixClient::FavouriteTag
                                     ? Expanded : Collapsed);
            view->setExpanded(i, expanded);
        }
    });
    connect( model, &RoomListModel::groupAdded, this, [this] (int pos) {
        view->expand(model->index(pos, 0));
    });
    setWidget(view);

    roomContextMenu = new QMenu(this);
    markAsReadAction =
        roomContextMenu->addAction(tr("Mark room as read"), this, [this] {
            if (auto room = getSelectedRoom())
                room->markAllMessagesAsRead();
        });
    roomContextMenu->addSeparator();
    addTagsAction =
        roomContextMenu->addAction(tr("Add tags..."), this,
                                   &RoomListDock::addTagsSelected);
    roomContextMenu->addSeparator();
    joinAction =
        roomContextMenu->addAction(tr("Join room"), this, [this] {
            if (auto room = getSelectedRoom())
            {
                Q_ASSERT(room->connection());
                room->connection()->joinRoom(room->id());
            }
        });
    leaveAction =
        roomContextMenu->addAction({}, this, [this] {
            if (auto room = getSelectedRoom())
                room->leaveRoom();
        });
    roomContextMenu->addSeparator();
    forgetAction =
        roomContextMenu->addAction(tr("Forget room"), this, [this] {
            if (auto room = getSelectedRoom())
            {
                Q_ASSERT(room->connection());
                room->connection()->forgetRoom(room->id());
            }
        });

    groupContextMenu = new QMenu(this);
    deleteTagAction =
        groupContextMenu->addAction(tr("Remove tag"), this, [this] {
            model->deleteTag(view->currentIndex());
        });

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &RoomListDock::showContextMenu);
}

void RoomListDock::addConnection(QMatrixClient::Connection* connection)
{
    model->addConnection(connection);
}

void RoomListDock::updateSortingMode()
{
//    const auto sortMode =
//            QMatrixClient::Settings().value("UI/sort_rooms_by", 0).toInt();
//    proxyModel->sort(sortMode,
//                     sortMode == 0 ? Qt::AscendingOrder : Qt::DescendingOrder);
    model->setOrder<OrderByTag>();
}

void RoomListDock::rowSelected(const QModelIndex& index)
{
    if (model->isValidRoomIndex(index))
//        emit roomSelected( model->roomAt(proxyModel->mapToSource(index)));
        emit roomSelected(model->roomAt(index));
}

void RoomListDock::showContextMenu(const QPoint& pos)
{
    auto index = view->indexAt(view->mapFromParent(pos));
    if (!index.isValid())
        return; // No context menu on root item yet
    if (model->isValidGroupIndex(index))
    {
        // Don't allow to delete system "tags"
        auto tagName = model->roomGroupAt(index);
        deleteTagAction->setDisabled(
                    tagName.toString().startsWith("org.qmatrixclient."));
        groupContextMenu->popup(mapToGlobal(pos));
        return;
    }
    Q_ASSERT(model->isValidRoomIndex(index));
    auto room = model->roomAt(index);
//    auto room = model->roomAt(proxyModel->mapToSource(index));

    using QMatrixClient::JoinState;
    bool joined = room->joinState() == JoinState::Join;
    bool invited = room->joinState() == JoinState::Invite;
    markAsReadAction->setEnabled(joined);
    addTagsAction->setEnabled(joined);
    joinAction->setEnabled(!joined);
    leaveAction->setText(invited ? tr("Reject invitation") : tr("Leave room"));
    leaveAction->setEnabled(room->joinState() != JoinState::Leave);
    forgetAction->setVisible(!invited);

    roomContextMenu->popup(mapToGlobal(pos));
}

QVariant RoomListDock::getSelectedGroup() const
{
    auto index = view->currentIndex();
    return !index.isValid() ? QVariant() : model->roomGroupAt(index);
}

QuaternionRoom* RoomListDock::getSelectedRoom() const
{
    QModelIndex index = view->currentIndex();
    return !index.isValid() || !index.parent().isValid() ? nullptr
                            : model->roomAt(index);
//                            : model->roomAt(proxyModel->mapToSource(index));
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
            tags[tag == tr("Favourites") ? QMatrixClient::FavouriteTag :
                 tag == tr("Low priority") ? QMatrixClient::LowPriorityTag :
                 tag.startsWith("m.") || tag.startsWith("u.") ? tag :
                 "u." + tag];
        }
        room->setTags(tags);
    }
}

void RoomListDock::refreshTitle()
{
    setWindowTitle(tr("Rooms (%1)").arg(model->totalRooms()));
}
