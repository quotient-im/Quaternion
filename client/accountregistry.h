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
    using const_iterator = QVector::const_iterator;
    using const_reference = QVector::const_reference;

    const QVector<Account*>& accounts() const { return *this; }
    void add(Account* a);
    void drop(Account* a);
    bool isLoggedIn(const QString& userId) const;
    const_iterator begin() const { return QVector::begin(); }
    const_iterator end() const { return QVector::end(); }
    const_reference front() const { return QVector::front(); }
    const_reference back() const { return QVector::back(); }
    using QVector::isEmpty, QVector::empty;
    using QVector::size, QVector::count, QVector::capacity;
    using QVector::cbegin, QVector::cend;
    using QVector::contains;

signals:
    void addedAccount(Account* a);
    void aboutToDropAccount(Account* a);
};

