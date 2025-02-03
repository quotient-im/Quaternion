#pragma once

#include "dialog.h"

namespace Quotient {
class KeyVerificationSession;
}

class VerificationDialog : public Dialog {
    Q_OBJECT
public:
    using Session = Quotient::KeyVerificationSession;

    VerificationDialog(Session* session, QWidget* parent);
    ~VerificationDialog() override;

private: // Overrides
    void buttonClicked(QAbstractButton* button) override;

private: // Data
    Session* session;
};
