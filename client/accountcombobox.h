#pragma once

#include <QtWidgets/QComboBox>

namespace Quotient {
class Connection;
}

class AccountComboBox : public QComboBox
{
    Q_OBJECT
public:
    using Account = Quotient::Connection;

    AccountComboBox(QVector<Account*> accounts, QWidget* parent = nullptr);

    void setAccount(Account* newAccount);
    Account* currentAccount() const;

signals:
    void currentAccountChanged(Account* newAccount);
};

