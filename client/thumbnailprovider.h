/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2016 Felix Rohrbach <kde@fxrh.de>              *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#pragma once

#include <QtQuick/QQuickAsyncImageProvider>
#include <QtCore/QAtomicPointer>

namespace Quotient {
    class Connection;
}

class ThumbnailProvider : public QQuickAsyncImageProvider {
public:
    explicit ThumbnailProvider(Quotient::Connection* connection = nullptr)
        : m_connection(connection)
    { }

    QQuickImageResponse* requestImageResponse(
        const QString& id, const QSize& requestedSize) override;

    void setConnection(Quotient::Connection* connection)
    {
        m_connection.storeRelaxed(connection);
    }

private:
    QAtomicPointer<Quotient::Connection> m_connection;
    Q_DISABLE_COPY(ThumbnailProvider)
};
