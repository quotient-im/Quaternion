#include "verificationdialog.h"

VerificationDialog::VerificationDialog(Quotient::Connection* account, const QString& deviceId,
                                       QWidget* parent)
    : Dialog(tr("Device verification"), QDialogButtonBox::NoButton, parent)
{
    //
}

VerificationDialog::~VerificationDialog() = default;
