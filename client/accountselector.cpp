#include "accountselector.h"

#include <connection.h>

using namespace Quotient;

AccountSelector::AccountSelector(QWidget *parent)
    : QComboBox(parent)
{
    connect(this, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] { emit currentAccountChanged(currentAccount()); });

    const auto& accounts = Accounts.accounts();
    for (auto* acc: accounts)
        addItem(acc->userId(), QVariant::fromValue(acc));

    connect(&Accounts, &AccountRegistry::rowsInserted, this, [this](const QModelIndex&, int first, int last) {
        for (int i = first; i < last; i++) {
            auto acc = Accounts.accounts()[i];
            if (const auto idx = indexOfAccount(acc); idx == -1)
                addItem(acc->userId(), QVariant::fromValue(acc));
            else
                qWarning()
                    << "AccountComboBox: refusing to add the same account twice";
            }
    });
    connect(&Accounts, &AccountRegistry::rowsAboutToBeRemoved, this,
            [this](const QModelIndex&, int first, int last) {
                for (int i = first; i < last; i++) {
                    auto acc = Accounts.accounts()[i];
                    if (const auto idx = indexOfAccount(acc); idx != -1)
                        removeItem(idx);
                    else
                        qWarning()
                            << "AccountComboBox: account to drop not found, ignoring";
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
    qWarning() << "AccountComboBox: account for"
               << newAccount->userId() + '/' + newAccount->deviceId()
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
