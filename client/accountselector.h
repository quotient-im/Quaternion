#pragma once

#include "accountregistry.h"

#include <QtWidgets/QComboBox>

class AccountSelector : public QComboBox
{
    Q_OBJECT
public:
    using Account = AccountRegistry::Account;

    AccountSelector(const AccountRegistry* registry, QWidget* parent = nullptr);

    void setAccount(Account* newAccount);
    Account* currentAccount() const;
    int indexOfAccount(Account* a) const;

signals:
    void currentAccountChanged(Account* newAccount);
};

