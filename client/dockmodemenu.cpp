#include "dockmodemenu.h"

#include <QtWidgets/QDockWidget>

DockModeMenu::DockModeMenu(QString name, QDockWidget* w)
    : QMenu(name)
    , dockWidget(w)
    , offAction(addAction(tr("&Off", "The dock panel is hidden"),
                          [this] { dockWidget->setVisible(false); }))
    , dockedAction(addAction(tr("&Docked"),
                             [this] {
                                 dockWidget->setVisible(true);
                                 dockWidget->setFloating(false);
                             }))
    , floatingAction(addAction(
          tr("&Floating", "The dock panel is floating, aka undocked"), [this] {
              dockWidget->setVisible(true);
              dockWidget->setFloating(true);
          }))
{
    offAction->setStatusTip(tr("Completely hide this list"));
    offAction->setCheckable(true);
    dockedAction->setStatusTip(tr("The list is shown within the main window"));
    dockedAction->setCheckable(true);
    floatingAction->setStatusTip(
        tr("The list is shown separately from the main window"));
    floatingAction->setCheckable(true);
    auto* radioGroup = new QActionGroup(this);
    for (auto* a : { offAction, dockedAction, floatingAction })
        radioGroup->addAction(a);
    connect(dockWidget, &QDockWidget::visibilityChanged, this,
            &DockModeMenu::updateMode);
    connect(dockWidget, &QDockWidget::topLevelChanged, this,
            &DockModeMenu::updateMode);
    updateMode();
}

void DockModeMenu::updateMode()
{
    if (dockWidget->isHidden())
        offAction->setChecked(true);
    else if (dockWidget->isFloating())
        floatingAction->setChecked(true);
    else
        dockedAction->setChecked(true);
}
