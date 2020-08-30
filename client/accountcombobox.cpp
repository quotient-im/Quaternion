#include "accountcombobox.h"

#include <connection.h>

AccountComboBox::AccountComboBox(QVector<Account *> accounts, QWidget *parent) : QComboBox(parent)
{
    for (auto* c: accounts)
        addItem(c->userId(), QVariant::fromValue(c));

    connect(this, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] { emit currentAccountChanged(currentAccount()); });
}

void AccountComboBox::setAccount(AccountComboBox::Account *newAccount)
{
    for (int i = 0; i < count(); ++i)
        if (itemData(i).value<Account*>() == newAccount) {
            setCurrentIndex(i);
            return;
        }
    Q_ASSERT(false);
    qWarning() << "AccountComboBox: account for"
               << newAccount->userId() + '/' + newAccount->deviceId()
               << "wasn't found in the full list of accounts";
}

AccountComboBox::Account* AccountComboBox::currentAccount() const
{
    return currentData().value<Account*>();
}
