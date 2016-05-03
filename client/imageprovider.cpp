/******************************************************************************
 * Copyright (C) 2016 Felix Rohrbach <kde@fxrh.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "imageprovider.h"
#include <jobs/mediathumbnailjob.h>

#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QWaitCondition>



ImageProvider::ImageProvider(QMatrixClient::Connection* connection)
    : QQuickImageProvider(QQmlImageProviderBase::Pixmap, QQmlImageProviderBase::ForceAsynchronousImageLoading)
    , m_connection(connection)
{
    qRegisterMetaType<QPixmap*>();
    qRegisterMetaType<QWaitCondition*>();
}

QPixmap ImageProvider::requestPixmap(const QString& id, QSize* size, const QSize& requestedSize)
{
    QMutexLocker locker(&m_mutex);
    qDebug() << "ImageProvider::requestPixmap:" << id;

    if( !m_connection )
    {
        qDebug() << "ImageProvider::requestPixmap: no connection!";
        return QPixmap();
    }

    QWaitCondition* condition = new QWaitCondition();
    QPixmap result;
    QMetaObject::invokeMethod(this, "doRequest", Qt::QueuedConnection,
                              Q_ARG(QString, id), Q_ARG(QSize, requestedSize),
                              Q_ARG(QPixmap*, &result), Q_ARG(QWaitCondition*, condition));
    condition->wait(&m_mutex);
    delete condition;

    if( size != 0 )
    {
        *size = result.size();
    }
    return result;
}

void ImageProvider::setConnection(QMatrixClient::Connection* connection)
{
    QMutexLocker locker(&m_mutex);

    m_connection = connection;
}

void ImageProvider::doRequest(QString id, QSize requestedSize, QPixmap* pixmap, QWaitCondition* condition)
{
    QMutexLocker locker(&m_mutex);

    int width = requestedSize.width() > 0 ? requestedSize.width() : 100;
    int height = requestedSize.height() > 0 ? requestedSize.height() : 100;

    QMatrixClient::MediaThumbnailJob* job = m_connection->getThumbnail(QUrl(id), width, height);
    QObject::connect( job, &QMatrixClient::MediaThumbnailJob::result, this, &ImageProvider::gotImage );
    ImageProviderData data = { pixmap, condition, requestedSize };
    m_callmap.insert(job, data);
}

void ImageProvider::gotImage(KJob* job)
{
    QMutexLocker locker(&m_mutex);
    qDebug() << "gotImage";

    auto mediaJob = static_cast<QMatrixClient::MediaThumbnailJob*>(job);
    ImageProviderData data = m_callmap.take(mediaJob);
    *data.pixmap = mediaJob->thumbnail().scaled(data.requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    data.condition->wakeAll();
}
