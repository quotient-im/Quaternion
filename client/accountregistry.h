#pragma once

#include <QtCore/QObject>
#include <QtCore/QVector>

namespace Quotient {
class Connection;
}

class AccountRegistry : public QObject, private QVector<Quotient::Connection*> {
    Q_OBJECT
public:
    using Account = Quotient::Connection;
    using storage_type = QVector<Account*>;
    using const_iterator = storage_type::const_iterator;
    using const_reference = storage_type::const_reference;

    const QVector<Account*>& accounts() const { return *this; }
    void add(Account* a);
    void drop(Account* a);
    bool isLoggedIn(const QString& userId) const;
    const_iterator begin() const { return storage_type::begin(); }
    const_iterator end() const { return storage_type::end(); }
    const_reference front() const { return storage_type::front(); }
    const_reference back() const { return storage_type::back(); }
    using storage_type::isEmpty, storage_type::empty;
    using storage_type::size, storage_type::count, storage_type::capacity;
    using storage_type::cbegin, storage_type::cend;
    using storage_type::contains;

signals:
    void addedAccount(Account* a);
    void aboutToDropAccount(Account* a);
};

