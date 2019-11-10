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
#include <QFontComboBox>

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

    // uiFont
    const auto curUIFontFamily = SettingsGroup("UI").value("Fonts/family", "").toString();
    uiFontFamily->insertItem(0, tr("system default"));
    if (curUIFontFamily == "") {
        uiFontFamily->setCurrentIndex(0);
        uiFontPointSize->setDisabled(true);
    } else {
        uiFontFamily->setCurrentIndex(uiFontFamily->findText(curUIFontFamily));
    }

    connect(uiFontFamily, &QFontComboBox::currentFontChanged, this, [this](const QFont &font) {
        QString value;
        if (font.family() == tr("system default")) {
            uiFontPointSize->setDisabled(true);
            SettingsGroup("UI").setValue("Fonts/pointSize", "");
            value = "";
        } else {
            uiFontPointSize->setDisabled(false);
            if (uiFontPointSize->currentIndex() == -1) {
                uiFontPointSize->setCurrentIndex(3);
                SettingsGroup("UI").setValue("Fonts/pointSize", "9");
            }
            value = font.family();
        }
        SettingsGroup("UI").setValue("Fonts/family", value);
    });

    for (int i = 6; i <= 18; i++)
        uiFontPointSize->addItem(QString::number(i));
    const auto curUIFontPointSize = SettingsGroup("UI").value("Fonts/pointSize", "");
    uiFontPointSize->setCurrentIndex(curUIFontPointSize.toInt() - 6);

    connect(uiFontPointSize, QOverload<const QString &>::of(&QComboBox::currentIndexChanged), this, [this](const QString &text) {
        SettingsGroup("UI").setValue("Fonts/pointSize", text);
    });

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

    // timelineFont
    const auto curTimelineFontFamily = SettingsGroup("UI").value("Fonts/timeline_family", "").toString();
    timelineFontFamily->insertItem(0, tr("system default"));
    if (curTimelineFontFamily == "") {
        timelineFontFamily->setCurrentIndex(0);
        timelineFontPointSize->setDisabled(true);
    } else {
        timelineFontFamily->setCurrentIndex(timelineFontFamily->findText(curTimelineFontFamily));
    }

    connect(timelineFontFamily, &QFontComboBox::currentFontChanged, this, [this](const QFont &font) {
        QString value;
        if (font.family() == tr("system default")) {
            timelineFontPointSize->setDisabled(true);
            SettingsGroup("UI").setValue("Fonts/timeline_pointSize", "");
            value = "";
        } else {
            timelineFontPointSize->setDisabled(false);
            if (timelineFontPointSize->currentIndex() == -1) {
                timelineFontPointSize->setCurrentIndex(3);
                SettingsGroup("UI").setValue("Fonts/timeline_pointSize", "9");
            }
            value = font.family();
        }
        SettingsGroup("UI").setValue("Fonts/timeline_family", value);
        emit timelineChanged();
    });

    for (int i = 6; i <= 18; i++)
        timelineFontPointSize->addItem(QString::number(i));
    const auto curTimelineFontPointSize = SettingsGroup("UI").value("Fonts/timeline_pointSize", "");
    timelineFontPointSize->setCurrentIndex(curTimelineFontPointSize.toInt() - 6);

    connect(timelineFontPointSize, QOverload<const QString &>::of(&QComboBox::currentIndexChanged), this, [this](const QString &text) {
        SettingsGroup("UI").setValue("Fonts/timeline_pointSize", text);
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
