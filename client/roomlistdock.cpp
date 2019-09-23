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
#include <QtWidgets/QLabel>
#include <QtWidgets/QPlainTextEdit>
#include <QtGui/QGuiApplication>

#include "mainwindow.h"
#include "models/roomlistmodel.h"
#include "models/orderbytag.h"
#include "quaternionroom.h"
#include "roomdialogs.h"
#include <connection.h>
#include <settings.h>

using Quotient::SettingsGroup;

class RoomListItemDelegate : public QStyledItemDelegate
{
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        void paint(QPainter *painter, const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};

void RoomListItemDelegate::paint(QPainter* painter,
         const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem o { option };

    if (!index.parent().isValid()) // Group captions
    {
        o.displayAlignment = Qt::AlignHCenter;
        o.font.setBold(true);
    }

    if (index.data(RoomListModel::HasUnreadRole).toBool())
        o.font.setBold(true);

    if (index.data(RoomListModel::HighlightCountRole).toInt() > 0)
    {
        static const auto highlightColor =
            Quotient::Settings().get("UI/highlight_color", QColor("orange"));
        o.palette.setColor(QPalette::Text, highlightColor);
        // Highlighting the text may not work out on monochrome colour schemes,
        // hence duplicating with italic font.
        o.font.setItalic(true);
    }

    const auto joinState = index.data(RoomListModel::JoinStateRole).toString();
    if (joinState == "invite")
        o.font.setItalic(true);
    else if (joinState == "leave" || joinState == "upgraded")
        o.font.setStrikeOut(true);

    QStyledItemDelegate::paint(painter, o, index);
}

RoomListDock::RoomListDock(MainWindow* parent)
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
    connect( view, &QTreeView::activated, this, &RoomListDock::rowSelected ); // See #608
    connect( view, &QTreeView::clicked, this, &RoomListDock::rowSelected);
    connect( view, &QTreeView::pressed, this, [this] {
        if (QGuiApplication::mouseButtons() & Qt::MidButton) {
            if (auto room = getSelectedRoom())
                room->markAllMessagesAsRead();
        }
    });
    connect( model, &RoomListModel::rowsInserted,
             this, &RoomListDock::refreshTitle );
    connect( model, &RoomListModel::rowsRemoved,
             this, &RoomListDock::refreshTitle );
    connect( model, &RoomListModel::saveCurrentSelection, this, [this] {
        selectedGroupCache = getSelectedGroup();
        selectedRoomCache = getSelectedRoom();
    });
    connect( model, &RoomListModel::restoreCurrentSelection, this, [this] {
        const auto& idx =
            model->indexOf(selectedGroupCache, selectedRoomCache);
//            proxyModel->mapFromSource(model->indexOf(selectedRoomCache));
        view->setCurrentIndex(idx);
        view->scrollTo(idx);
        selectedGroupCache.clear();
        selectedRoomCache = nullptr;
    });

    static SettingsGroup dockSettings("UI/RoomsDock");
    connect(model, &RoomListModel::groupAdded, this, [this](int groupPos) {
        const auto& i = model->index(groupPos, 0);
        const auto groupKey = model->roomGroupAt(i).toString();
        if (groupKey.startsWith("org.qmatrixclient"))
            qCritical() << groupKey << "is deprecated!"; // Fighting the legacy
        auto groupState = dockSettings.value(groupKey);
        if (!groupState.isValid()) {
            if (groupKey.startsWith(RoomGroup::SystemPrefix)) {
                const auto legacyKey = RoomGroup::LegacyPrefix
                                       + groupKey.mid(
                                           RoomGroup::SystemPrefix.size());
                groupState = dockSettings.value(legacyKey);
                dockSettings.setValue(groupKey, groupState);
                if (groupState.isValid())
                    dockSettings.remove(legacyKey);
            }
        }
        view->setExpanded(i, groupState.isValid()
                                 ? groupState.toString() == Expanded
                                 : groupKey == Quotient::FavouriteTag);
    });
    connect(view, &QTreeView::expanded, this, [this](QModelIndex i) {
        dockSettings.setValue(model->roomGroupAt(i).toString(), Expanded);
    });
    connect(view, &QTreeView::collapsed, this, [this](QModelIndex i) {
        dockSettings.setValue(model->roomGroupAt(i).toString(), Collapsed);
    });

    setWidget(view);

    roomContextMenu = new QMenu(this);
    markAsReadAction =
        roomContextMenu->addAction(QIcon::fromTheme("mail-mark-read"),
            tr("Mark room as read"), this, [this] {
            if (auto room = getSelectedRoom())
                room->markAllMessagesAsRead();
        });
    roomContextMenu->addSeparator();
    addTagsAction =
        roomContextMenu->addAction(QIcon::fromTheme("tag-new"),
        tr("Add tags..."), this, &RoomListDock::addTagsSelected);
    roomSettingsAction =
        roomContextMenu->addAction(QIcon::fromTheme("user-group-properties"),
            tr("Change room &settings..."), [this,parent]
            {
                auto* dlg = new RoomSettingsDialog(getSelectedRoom(), parent);
                dlg->setModal(false);
                dlg->setAttribute(Qt::WA_DeleteOnClose);
                dlg->reactivate();
            });
    roomContextMenu->addSeparator();
    joinAction =
        roomContextMenu->addAction(QIcon::fromTheme("irc-join-channel"),
        tr("Join room"), this, [this] {
            if (auto room = getSelectedRoom())
            {
                Q_ASSERT(room->connection());
                room->connection()->joinRoom(room->id());
            }
        });
    leaveAction =
        roomContextMenu->addAction(QIcon::fromTheme("irc-close-channel"),
        {}, this, [this] {
            if (auto room = getSelectedRoom())
                room->leaveRoom();
        });
    roomContextMenu->addSeparator();
    forgetAction =
        roomContextMenu->addAction(QIcon::fromTheme("irc-remove-operator"),
        tr("Forget room"), this, [this] {
            if (auto room = getSelectedRoom())
            {
                Q_ASSERT(room->connection());
                room->connection()->forgetRoom(room->id());
            }
        });

    groupContextMenu = new QMenu(this);
    deleteTagAction =
        groupContextMenu->addAction(QIcon::fromTheme("tag-delete"),
        tr("Remove tag"), this, [this] {
            model->deleteTag(view->currentIndex());
        });

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &RoomListDock::showContextMenu);
}

void RoomListDock::addConnection(Quotient::Connection* connection)
{
    model->addConnection(connection);
}

void RoomListDock::updateSortingMode()
{
//    const auto sortMode =
//            Quotient::Settings().value("UI/sort_rooms_by", 0).toInt();
//    proxyModel->sort(sortMode,
//                     sortMode == 0 ? Qt::AscendingOrder : Qt::DescendingOrder);
    model->setOrder<OrderByTag>();
}

void RoomListDock::setSelectedRoom(QuaternionRoom* room)
{
    if (getSelectedRoom() == room)
        return;
    // First try the current group; if that fails, try the entire list
    QModelIndex idx;
    auto currentGroup = getSelectedGroup();
    if (!currentGroup.isNull())
        idx = model->indexOf(currentGroup, room);
    if (!idx.isValid())
        idx = model->indexOf({}, room);
    if (idx.isValid())
    {
        view->setCurrentIndex(idx);
        view->scrollTo(idx);
    }
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
            tagName.toString().startsWith(RoomGroup::SystemPrefix));
        groupContextMenu->popup(mapToGlobal(pos));
        return;
    }
    Q_ASSERT(model->isValidRoomIndex(index));
    auto room = model->roomAt(index);
//    auto room = model->roomAt(proxyModel->mapToSource(index));

    using Quotient::JoinState;
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
        Dialog dlg(tr("Enter new tags for the room"), this, Dialog::NoStatusLine,
                   tr("Add", "A caption on a button to add tags"),
                   Dialog::NoExtraButtons);
        dlg.addWidget(
            new QLabel(tr("Enter tags to add to this room, one tag per line")));
        auto tagsInput = new QPlainTextEdit();
        tagsInput->setTabChangesFocus(true);
        dlg.addWidget(tagsInput);
        if (dlg.exec() != QDialog::Accepted)
            return;

        auto tags = room->tags();
        const auto enteredTags =
                tagsInput->toPlainText().split('\n', QString::SkipEmptyParts);
        for (const auto& tag: enteredTags)
            tags[captionToTag(tag)]; // No overwriting, just ensure existence

        room->setTags(tags);
    }
}

void RoomListDock::refreshTitle()
{
    setWindowTitle(tr("Rooms (%1)").arg(model->totalRooms()));
}
