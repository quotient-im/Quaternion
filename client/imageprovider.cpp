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
#include <jobs/mediathumbnailjob.h>

#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QWaitCondition>

#include <QtCore/QDebug>

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

    QWaitCondition condition;
    QPixmap result;
    QMetaObject::invokeMethod(this, "doRequest", Qt::QueuedConnection,
                              Q_ARG(QString, id), Q_ARG(QSize, requestedSize),
                              Q_ARG(QPixmap*, &result), Q_ARG(QWaitCondition*, &condition));
    condition.wait(&m_mutex);

    if( size != nullptr )
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

    if( !m_connection )
    {
        qDebug() << "ImageProvider::requestPixmap: no connection!";
        *pixmap = QPixmap();
        condition->wakeAll();
    }

    auto job = m_connection->getThumbnail(QUrl(id), requestedSize.expandedTo({100,100}));
    connect( job, &QMatrixClient::MediaThumbnailJob::success, this, &ImageProvider::gotImage );
    ImageProviderData data = { pixmap, condition, requestedSize };
    m_callmap.insert(job, data);
}

void ImageProvider::gotImage(QMatrixClient::BaseJob* job)
{
    QMutexLocker locker(&m_mutex);
    qDebug() << "gotImage";

    auto mediaJob = static_cast<QMatrixClient::MediaThumbnailJob*>(job);
    ImageProviderData data = m_callmap.take(mediaJob);
    *data.pixmap = mediaJob->thumbnail().scaled(data.requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    data.condition->wakeAll();
}
