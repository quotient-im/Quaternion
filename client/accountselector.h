#pragma once

#include <QtWidgets/QComboBox>

namespace Quotient {
class AccountRegistry;
class Connection;
}

class AccountSelector : public QComboBox
{
    Q_OBJECT
public:
    AccountSelector(Quotient::AccountRegistry *registry,
                    QWidget* parent = nullptr);

    void setAccount(Quotient::Connection* newAccount);
    Quotient::Connection* currentAccount() const;
    int indexOfAccount(Quotient::Connection* a) const;

signals:
    void currentAccountChanged(Quotient::Connection* newAccount);
};

