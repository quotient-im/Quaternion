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

#include "lib/connection.h"
#include "jobs/mediathumbnailjob.h"

#include <QtCore/QWaitCondition>
#include <QtCore/QDebug>

using QMatrixClient::MediaThumbnailJob;

ImageProvider::ImageProvider(QMatrixClient::Connection* connection)
    : QQuickImageProvider(QQmlImageProviderBase::Image,
                          QQmlImageProviderBase::ForceAsynchronousImageLoading)
    , m_connection(connection)
{
#if (QT_VERSION < QT_VERSION_CHECK(5, 10, 0))
    qRegisterMetaType<MediaThumbnailJob*>();
#endif
}

QImage ImageProvider::requestImage(const QString& id,
                                   QSize* pSize, const QSize& requestedSize)
{
    QUrl mxcUri { "mxc://" + id };
    qDebug() << "ImageProvider::requestPixmap:" << mxcUri.toString();

    MediaThumbnailJob* job = nullptr;
    QReadLocker locker(&m_lock);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
    QMetaObject::invokeMethod(m_connection,
        [=] { return m_connection->getThumbnail(mxcUri, requestedSize); },
        Qt::BlockingQueuedConnection, &job);
#else
    QMetaObject::invokeMethod(m_connection, "getThumbnail",
        Qt::BlockingQueuedConnection, Q_RETURN_ARG(MediaThumbnailJob*, job),
        Q_ARG(QUrl, mxcUri), Q_ARG(QSize, requestedSize));
#endif
    if (!job)
    {
        qDebug() << "ImageProvider: failed to send a request";
        return {};
    }
    QImage result;
    {
        QWaitCondition condition; // The most compact way to block on a signal
        job->connect(job, &MediaThumbnailJob::finished, job, [&] {
            result = job->thumbnail();
            condition.wakeAll();
        });
        condition.wait(&m_lock);
    }

    if( pSize != nullptr )
        *pSize = result.size();

    return result;
}

void ImageProvider::setConnection(QMatrixClient::Connection* connection)
{
    QWriteLocker locker(&m_lock);

    m_connection = connection;
}
