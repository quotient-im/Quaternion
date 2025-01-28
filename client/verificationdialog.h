#pragma once

#include "dialog.h"

class VerificationDialog : public Dialog {
    Q_OBJECT
public:
    VerificationDialog(Quotient::Connection* account, const QString& deviceId, QWidget* parent);
    ~VerificationDialog() override;
};
