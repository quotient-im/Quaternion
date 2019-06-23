/**************************************************************************
 *                                                                        *
 * Copyright (C) 2019 Roland Pallai <dap78@magex.hu>                      *
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

#include "settingsdialog.h"
#include "mainwindow.h"
#include <settings.h>

#include <QTabWidget>
#include <QVBoxLayout>
#include <QDialog>

using Quotient::SettingsGroup;

SettingsDialog::SettingsDialog(MainWindow *parent) : QDialog(parent) {
    setWindowTitle(tr("Settings"));

    QVBoxLayout *layout = new QVBoxLayout(this);
    stack = new QTabWidget(this);
    stack->setTabBarAutoHide(true);
    layout->addWidget(stack);
    stack->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

    auto generalPage = new GeneralPage(this);
    connect(generalPage, &GeneralPage::timelineChanged, this, [this] {
        emit timelineChanged();
    });
    addPage(generalPage, tr("&General"));
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::addPage(ConfigurationWidgetInterface *page, const QString &title)
{
    stack->addTab(page->asWidget(), title);
    pages << page;
}

//
// GeneralPage
//
GeneralPage::GeneralPage(SettingsDialog *parent):
    QWidget(parent), Ui_GeneralPage(), m_parent(parent)
{
    Ui_GeneralPage::setupUi(this);

    // closeToTray
    closeToTray->setChecked(SettingsGroup("UI").value("close_to_tray", false).toBool());
    connect(closeToTray, &QAbstractButton::toggled, this, [=](bool checked) {
        SettingsGroup("UI").setValue("close_to_tray", checked);
    });

    // timeline_layout
    const auto curTimelineStyle = SettingsGroup("UI").value("timeline_style", "default");
    timeline_layout->addItem(tr("Default"), QVariant(QStringLiteral("default")));
    timeline_layout->addItem(tr("XChat"), QVariant(QStringLiteral("xchat")));
    if (curTimelineStyle == "xchat")
        timeline_layout->setCurrentText(tr("XChat"));

    connect(timeline_layout, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        auto new_layout = timeline_layout->itemData(index).toString();
        SettingsGroup("UI").setValue("timeline_style", new_layout);
        emit timelineChanged();
    });

    // useShuttleScrollbar
    useShuttleScrollbar->setChecked(SettingsGroup("UI").value("use_shuttle_dial", true).toBool());
    connect(useShuttleScrollbar, &QAbstractButton::toggled, this, [=](bool checked) {
        SettingsGroup("UI").setValue("use_shuttle_dial", checked);
        // needs restart instead
        //emit timelineChanged();
    });

    // loadFullSizeImages
    loadFullSizeImages->setChecked(SettingsGroup("UI").value("autoload_images", true).toBool());
    connect(loadFullSizeImages, &QAbstractButton::toggled, this, [=](bool checked) {
        SettingsGroup("UI").setValue("autoload_images", checked);
        emit timelineChanged();
    });

    // *Notif
    const auto curNotifications = SettingsGroup("UI").value("notifications", "intrusive");
    if (curNotifications == "intrusive") {
        fullNotif->setChecked(true);
    } else if (curNotifications == "non-intrusive") {
        gentleNotif->setChecked(true);
    } else if (curNotifications == "none") {
        noNotif->setChecked(true);
    }

    connect(noNotif, &QAbstractButton::clicked, this, [=]() {
        SettingsGroup("UI").setValue("notifications", "none");
    });
    connect(gentleNotif, &QAbstractButton::clicked, this, [=]() {
        SettingsGroup("UI").setValue("notifications", "non-intrusive");
    });
    connect(fullNotif, &QAbstractButton::clicked, this, [=]() {
        SettingsGroup("UI").setValue("notifications", "intrusive");
    });
}

QWidget *GeneralPage::asWidget()
{
    return this;
}
