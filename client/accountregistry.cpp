#include "accountregistry.h"

#include <connection.h>

void AccountRegistry::add(AccountRegistry::Account* a)
{
    if (contains(a))
        return;
    push_back(a);
    emit addedAccount(a);
}

void AccountRegistry::drop(Account* a)
{
    emit aboutToDropAccount(a);
    removeOne(a);
    Q_ASSERT(!contains(a));
}

bool AccountRegistry::isLoggedIn(const QString &userId) const
{
    return std::any_of(cbegin(), cend(),
                       [&userId](Account* a) { return a->userId() == userId; });
}
