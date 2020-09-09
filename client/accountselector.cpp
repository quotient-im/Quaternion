#include "accountselector.h"

#include <connection.h>

AccountSelector::AccountSelector(const AccountRegistry *registry, QWidget *parent)
    : QComboBox(parent)
{
    connect(this, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] { emit currentAccountChanged(currentAccount()); });

    const auto& accounts = registry->accounts();
    for (auto* acc: accounts)
        addItem(acc->userId(), QVariant::fromValue(acc));

    connect(registry, &AccountRegistry::addedAccount, this, [this](Account* acc) {
        if (const auto idx = indexOfAccount(acc); idx == -1)
            addItem(acc->userId(), QVariant::fromValue(acc));
        else
            qWarning()
                << "AccountComboBox: refusing to add the same account twice";
    });
    connect(registry, &AccountRegistry::aboutToDropAccount, this,
            [this](Account* acc) {
                if (const auto idx = indexOfAccount(acc); idx != -1)
                    removeItem(idx);
                else
                    qWarning()
                        << "AccountComboBox: account to drop not found, ignoring";
            });
}

void AccountSelector::setAccount(Account *newAccount)
{
    if (auto i = indexOfAccount(newAccount); i != -1) {
        setCurrentIndex(i);
        return;
    }
    Q_ASSERT(false);
    qWarning() << "AccountComboBox: account for"
               << newAccount->userId() + '/' + newAccount->deviceId()
               << "wasn't found in the full list of accounts";
}

AccountSelector::Account* AccountSelector::currentAccount() const
{
    return currentData().value<Account*>();
}

int AccountSelector::indexOfAccount(Account* a) const
{
    for (int i = 0; i < count(); ++i)
        if (itemData(i).value<Account*>() == a)
            return i;

    return -1;
}
