#pragma once

#include <accountregistry.h>

#include <QtWidgets/QComboBox>

namespace Quotient
{
    class Connection;
}

class AccountSelector : public QComboBox
{
    Q_OBJECT
public:
    AccountSelector(QWidget* parent = nullptr);

    void setAccount(Quotient::Connection* newAccount);
    Quotient::Connection* currentAccount() const;
    int indexOfAccount(Quotient::Connection* a) const;

signals:
    void currentAccountChanged(Quotient::Connection* newAccount);
};

