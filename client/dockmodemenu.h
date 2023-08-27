#pragma once

#include <QtWidgets/QMenu>

class QDockWidget;

class DockModeMenu : public QMenu {
    Q_OBJECT
public:
    DockModeMenu(QString name, QDockWidget* w);

private slots:
    void updateMode();

private:
    QDockWidget* dockWidget;
    QAction* offAction;
    QAction* dockedAction;
    QAction* floatingAction;
};
