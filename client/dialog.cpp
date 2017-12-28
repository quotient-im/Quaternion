#include "dialog.h"

Dialog::Dialog(const QString& title, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(title);

#if (QT_VERSION < QT_VERSION_CHECK(5, 10, 0))
    setWindowFlags(windowFlags()&~Qt::WindowContextHelpButtonHint);
#endif
}
