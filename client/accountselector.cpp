#include "accountselector.h"

#include "logging_categories.h"

#include <Quotient/accountregistry.h>
#include <Quotient/connection.h>

using namespace Quotient;

AccountSelector::AccountSelector(AccountRegistry* registry, QWidget* parent)
    : QComboBox(parent)
{
    Q_ASSERT(registry != nullptr);
    connect(this, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] { emit currentAccountChanged(currentAccount()); });

    for (auto* acc: registry->accounts())
        addItem(acc->userId(), QVariant::fromValue(acc));

    connect(registry, &AccountRegistry::rowsInserted, this,
            [this, registry](const QModelIndex&, int first, int last) {
                const auto& accounts = registry->accounts();
                for (int i = first; i < last; i++) {
                    auto acc = accounts[i];
                    if (const auto idx = indexOfAccount(acc); idx == -1)
                        addItem(acc->userId(), QVariant::fromValue(acc));
                    else
                        qCWarning(ACCOUNTSELECTOR)
                            << "Refusing to add the same account twice";
                }
            });
    connect(registry, &AccountRegistry::rowsAboutToBeRemoved, this,
            [this, registry](const QModelIndex&, int first, int last) {
                const auto& accounts = registry->accounts();
                for (int i = first; i < last; i++) {
                    auto acc = accounts[i];
                    if (const auto idx = indexOfAccount(acc); idx != -1)
                        removeItem(idx);
                    else
                        qCWarning(ACCOUNTSELECTOR)
                            << "Account to drop not found, ignoring";
                }
            });
}

void AccountSelector::setAccount(Connection *newAccount)
{
    if (!newAccount) {
        setCurrentIndex(-1);
        return;
    }
    if (auto i = indexOfAccount(newAccount); i != -1) {
        setCurrentIndex(i);
        return;
    }
    Q_ASSERT(false);
    qCWarning(ACCOUNTSELECTOR)
        << "Account for" << newAccount->userId() + '/' + newAccount->deviceId()
        << "wasn't found in the full list of accounts";
}

Connection* AccountSelector::currentAccount() const
{
    return currentData().value<Connection*>();
}

int AccountSelector::indexOfAccount(Connection* a) const
{
    for (int i = 0; i < count(); ++i)
        if (itemData(i).value<Connection*>() == a)
            return i;

    return -1;
}
