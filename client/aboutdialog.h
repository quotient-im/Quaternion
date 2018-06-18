#pragma once

#include "dialog.h"
#include "gitinfo.h"

class AboutDialog : public Dialog
{
        Q_OBJECT
    public:
        explicit AboutDialog(QWidget* parent = nullptr);
        ~AboutDialog() override;
};