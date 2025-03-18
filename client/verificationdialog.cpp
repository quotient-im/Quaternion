#include "verificationdialog.h"

#include <Quotient/keyverificationsession.h>

#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

#include <QtCore/QStringBuilder>

using namespace Qt::StringLiterals;

VerificationDialog::VerificationDialog(Session* session, QWidget* parent)
    : Dialog(tr("Verifying device %1").arg(session->remoteDeviceId()),
             QDialogButtonBox::Ok | QDialogButtonBox::Discard, parent)
    , session(session)
{
    // The same check as in Session::handleEvent() in the KeyVerificationKeyEvent branch
    QUO_CHECK(session->state() == Session::WAITINGFORKEY || session->state() == Session::ACCEPTED);

    addWidget(new QLabel(
      tr("Confirm that the same icons, in the same order are displayed on the other side")));
    const auto emojis = session->sasEmojis();
    constexpr auto rowsCount = 2;
    const auto rowSize = (emojis.size() + 1) / rowsCount;
    auto emojiGrid = addLayout<QHBoxLayout>();
    for (int i = 0; i < emojis.size(); ++i) {
        auto emojiLayout = new QVBoxLayout();
        auto emoji = new QLabel(emojis[i].emoji);
        emoji->setFont({ u"emoji"_s, emoji->font().pointSize() * 4 });
        for (auto* const l : { emoji, new QLabel(emojis[i].description) }) {
            emojiLayout->addWidget(l);
            emojiLayout->setAlignment(l, Qt::AlignCenter);
        }
        emojiGrid->addLayout(emojiLayout);
        if (i % rowSize == rowSize - 1)
            emojiGrid = addLayout<QHBoxLayout>(); // Start new line
    }
    button(QDialogButtonBox::Ok)->setText(tr("They match"));
    button(QDialogButtonBox::Discard)->setText(tr("They DON'T match"));

    // Pin lifecycles of the dialog and the session, avoiding recursion (by the time
    // QObject::destroyed is emitted, KeyVerificationSession signals are disconnected)
    connect(session, &QObject::destroyed, this, &QDialog::reject);
    // NB: this is only triggered when a dialog is closed using a window close button;
    //     QDialogButtonBox::Discard doesn't trigger QDialog::rejected as it has DestructiveRole
    connect(this, &QDialog::rejected, session, [session] {
        if (session->state() != Session::CANCELED)
            session->cancelVerification(Session::USER);
    });
}

VerificationDialog::~VerificationDialog() = default;

void VerificationDialog::buttonClicked(QAbstractButton* button)
{
    if (button == this->button(QDialogButtonBox::Ok)) {
        session->sendMac();
        accept();
    } else if (button == this->button(QDialogButtonBox::Discard)) {
        session->cancelVerification(Session::MISMATCHED_SAS);
        reject();
    } else
        QUO_ALARM_X(false, "Unknown button: " % button->text());
}
