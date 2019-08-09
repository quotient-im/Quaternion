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

#include <QtCore/QReadWriteLock>
#include <QtCore/QThread>
#include <QtCore/QDebug>

using Quotient::Connection;
using Quotient::BaseJob;

class ThumbnailResponse : public QQuickImageResponse
{
        Q_OBJECT
    public:
        ThumbnailResponse(Connection* c, QString id, QSize size)
            : c(c), mediaId(std::move(id)), requestedSize(size)
            , errorStr(tr("Image request hasn't started"))
        {
            if (!c)
            {
                errorStr = tr("No connection to perform image request");
                emit finished();
                return;
            }
            if (mediaId.count('/') != 1)
            {
                errorStr =
                    tr("Media id '%1' doesn't follow server/mediaId pattern")
                    .arg(mediaId);
                emit finished();
                return;
            }
            // Execute a request on the main thread asynchronously
            moveToThread(c->thread());
            QMetaObject::invokeMethod(this,
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
                &ThumbnailResponse::startRequest
#else
                "startRequest"
#endif
            );
        }
        ~ThumbnailResponse() override = default;

    private slots:
        // All these run in the main thread, not QML thread

        void startRequest()
        {
            Q_ASSERT(QThread::currentThread() == c->thread());

            job = c->getThumbnail(mediaId, requestedSize);
            // Connect to any possible outcome including abandonment
            // to make sure the QML thread is not left stuck forever.
            connect(job, &BaseJob::finished,
                    this, &ThumbnailResponse::prepareResult);
        }

        void prepareResult()
        {
            Q_ASSERT(QThread::currentThread() == job->thread());
            Q_ASSERT(job->error() != BaseJob::Pending);
            {
                QWriteLocker _(&lock);
                if (job->error() == BaseJob::Success)
                {
                    image = job->thumbnail();
                    errorStr.clear();
                    qDebug() << "ThumbnailResponse: image ready for" << mediaId;
                } else if (job->error() == BaseJob::Abandoned) {
                    errorStr = tr("Image request has been cancelled");
                    qDebug() << "ThumbnailResponse: cancelled for" << mediaId;
                } else {
                    errorStr = job->errorString();
                    qWarning() << "ThumbnailResponse: no valid image for" << mediaId
                               << "-" << errorStr;
                }
            }
            job = nullptr;
            emit finished();
        }

        void doCancel()
        {
            if (job)
            {
                Q_ASSERT(QThread::currentThread() == job->thread());
                job->abandon();
            }
        }

    private:
        Connection* c;
        const QString mediaId;
        const QSize requestedSize;
        Quotient::MediaThumbnailJob* job = nullptr;

        QImage image;
        QString errorStr;
        mutable QReadWriteLock lock; // Guards ONLY these two above

        // The following overrides run in QML thread

        QQuickTextureFactory *textureFactory() const override
        {
            QReadLocker _(&lock);
            return QQuickTextureFactory::textureFactoryForImage(image);
        }

        QString errorString() const override
        {
            QReadLocker _(&lock);
            return errorStr;
        }

        void cancel() override
        {
            // Flip from QML thread to the main thread
            QMetaObject::invokeMethod(this,
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
                &ThumbnailResponse::doCancel
#else
                "doCancel"
#endif
            );
        }
};

#include "imageprovider.moc" // Because we define a Q_OBJECT in the cpp file

ImageProvider::ImageProvider(Connection* connection)
    : m_connection(connection)
{ }

QQuickImageResponse* ImageProvider::requestImageResponse(
        const QString& id, const QSize& requestedSize)
{
    qDebug() << "ImageProvider: requesting " << id;
    return new ThumbnailResponse(m_connection.load(), id, requestedSize);
}

void ImageProvider::setConnection(Connection* connection)
{
    m_connection.store(connection);
}
