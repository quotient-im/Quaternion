/**************************************************************************
 *                                                                        *
 * Copyright (C) 2016 Felix Rohrbach <kde@fxrh.de>                        *
 *                                                                        *
 * This program is free software; you can redistribute it and/or          *
 * modify it under the terms of the GNU General Public License            *
 * as published by the Free Software Foundation; either version 3         *
 * of the License, or (at your option) any later version.                 *
 *                                                                        *
 * This program is distributed in the hope that it will be useful,        *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 * GNU General Public License for more details.                           *
 *                                                                        *
 * You should have received a copy of the GNU General Public License      *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 *                                                                        *
 **************************************************************************/

#include "imageprovider.h"

#include <connection.h>
#include <jobs/mediathumbnailjob.h>

#include <QtCore/QWaitCondition>
#include <QtCore/QDebug>

using QMatrixClient::Connection;

class Response : public QQuickImageResponse
{
    public:
        Response(Connection* c, QString mediaId, const QSize& requestedSize)
            : c(c), mediaId(std::move(mediaId)), requestedSize(requestedSize)
        {
            moveToThread(c->thread());
        }
        ~Response() override { if (job) job->abandon(); }

        void startRequest()
        {
            job = c->getThumbnail(mediaId, requestedSize);
            // Connect to any possible outcome including abandonment to make sure
            // the QML thread is not left stuck forever.
            connect(job, &QMatrixClient::BaseJob::finished,
                    this, &QQuickImageResponse::finished);
        }

    private:
        Connection* c;
        const QString mediaId;
        const QSize requestedSize;
        QPointer<QMatrixClient::MediaThumbnailJob> job;

        QQuickTextureFactory *textureFactory() const override
        {
            if (job)
                return QQuickTextureFactory::textureFactoryForImage(job->thumbnail());

            qWarning() << "Empty job for image" << mediaId;
            return nullptr;
        }

        QString errorString() const override
        {
            return !job || job->status().good() ? QString() : job->errorString();
        }

        void cancel() override
        {
            if (job)
                job->abandon();
        }
};

ImageProvider::ImageProvider(Connection* connection)
    : m_connection(connection)
{ }

QQuickImageResponse* ImageProvider::requestImageResponse(
        const QString& id, const QSize& requestedSize)
{
    if (id.count('/') != 1)
    {
        qWarning() << "ImageProvider: won't fetch an invalid id:" << id
                   << "doesn't follow server/mediaId pattern";
        return nullptr;
    }

    qDebug() << "ImageProvider: requesting " << id;
    auto r = new Response(m_connection.load(), id, requestedSize);
    // Execute a request on the main thread asynchronously
    QMetaObject::invokeMethod(r,
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
        &Response::startRequest,
#else
        "startRequest",
#endif
        Qt::QueuedConnection);
    return r;
}

void ImageProvider::setConnection(Connection* connection)
{
    m_connection.store(connection);
}
