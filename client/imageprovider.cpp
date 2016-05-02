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

ImageProvider::ImageProvider(QMatrixClient::Connection* connection, QThread* mainThread)
    : m_connection(connection)
    , m_mainThread(mainThread)
{
}

QQuickImageResponse* ImageProvider::requestImageResponse(const QString& id, const QSize& requestedSize)
{
    QMutexLocker locker(&m_mutex);

    QSize size;
    size.setWidth(requestedSize.width() > 0 ? requestedSize.width() : 100);
    size.setHeight(requestedSize.height() > 0 ? requestedSize.height() : 100);

    QuaternionImageResponse* res = new QuaternionImageResponse(m_connection, id, size);
    res->moveToThread(m_mainThread);
    QMetaObject::invokeMethod(res, "requestImage"); // to make sure it's called in the other thread
    return res;
}

void ImageProvider::setConnection(QMatrixClient::Connection* connection)
{
    QMutexLocker locker(&m_mutex);

    m_connection = connection;
}

QuaternionImageResponse::QuaternionImageResponse(QMatrixClient::Connection* connection, const QString& id, const QSize& requestedSize)
    : m_connection(connection)
    , m_id(id)
    , m_requestedSize(requestedSize)
{
}

QQuickTextureFactory* QuaternionImageResponse::textureFactory() const
{
    qDebug() << "textureFactory";
    return QQuickTextureFactory::textureFactoryForImage(m_result);
}

QString QuaternionImageResponse::errorString() const
{
    return m_errorString;
}

void QuaternionImageResponse::gotImage()
{
    m_result = m_job->thumbnail().scaled(m_requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation).toImage();
    emit finished();
}

void QuaternionImageResponse::requestImage()
{
    if( !m_connection )
    {
        m_errorString = "0-Connection";
        qDebug() << "QuaternionImageResponse::requestImage: no connection!";
        emit finished();
        return;
    }
    m_job = m_connection->getThumbnail(QUrl(m_id), m_requestedSize.width(), m_requestedSize.height());
    connect(m_job, &QMatrixClient::MediaThumbnailJob::finished, this, &QuaternionImageResponse::gotImage);
}