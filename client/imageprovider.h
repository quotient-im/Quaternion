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

// FIXME: It's actually ThumbnailProvider, not ImageProvider, because internally
// it calls MediaThumbnailJob. Trying to get a full-size image using this
// provider may bring suboptimal results (and generally you shouldn't do it
// because images loaded by QML are not necessarily cached to disk, so it's a
// waste of bandwidth).

class ImageProvider: public QQuickAsyncImageProvider
{
    public:
        explicit ImageProvider(Quotient::Connection* connection = nullptr);

        QQuickImageResponse* requestImageResponse(
                const QString& id, const QSize& requestedSize) override;

        void setConnection(Quotient::Connection* connection);

    private:
        QAtomicPointer<Quotient::Connection> m_connection;
        Q_DISABLE_COPY(ImageProvider)
};
