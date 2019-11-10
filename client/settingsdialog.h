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

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include "ui_settingsgeneralpage.h"

#include <QDialog>

class SettingsDialog;
class MainWindow;
class QTabWidget;

/** Common interface for settings' tabs
 */
class ConfigurationWidgetInterface {
public:
    virtual QWidget *asWidget() = 0;
};

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    SettingsDialog(MainWindow *parent);
    ~SettingsDialog() override;

private:
    QTabWidget *stack;
    QVector<ConfigurationWidgetInterface*> pages;

    void addPage(ConfigurationWidgetInterface *page, const QString &title);

signals:
    // it should be the job of the model
    void timelineChanged();
};

//
// GeneralPage
//
class GeneralPage : public QWidget, Ui_GeneralPage, public ConfigurationWidgetInterface
{
    Q_OBJECT
public:
    GeneralPage(SettingsDialog *parent);
    virtual QWidget *asWidget();

private:
    SettingsDialog *m_parent;

signals:
    void timelineChanged();
};

#endif /* SETTINGSDIALOG_H */
