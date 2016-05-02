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

#ifndef IMAGEPROVIDER_H
#define IMAGEPROVIDER_H

#include <QtQuick/QQuickAsyncImageProvider>
#include <QtQuick/QQuickImageResponse>
#include <QtCore/QThread>
#include <QtCore/QMutex>

#include "quaternionconnection.h"

class ImageProvider: public QQuickAsyncImageProvider
{
    public:
        ImageProvider(QMatrixClient::Connection* connection, QThread* mainThread);

        QQuickImageResponse* requestImageResponse(const QString& id, const QSize& requestedSize);

        void setConnection(QMatrixClient::Connection* connection);

    private:
        QMatrixClient::Connection* m_connection;
        QThread* m_mainThread;
        QMutex m_mutex;
};

class QuaternionImageResponse: public QQuickImageResponse
{
        Q_OBJECT
    public:
        QuaternionImageResponse(QMatrixClient::Connection* connection, const QString& id, const QSize& requestedSize);

        QQuickTextureFactory* textureFactory() const;

        QString errorString() const;

    private slots:
        void gotImage();

    private:
        Q_INVOKABLE void requestImage();

        QMatrixClient::Connection* m_connection;
        QMatrixClient::MediaThumbnailJob* m_job;
        QString m_id;
        QSize m_requestedSize;
        QString m_errorString;
        QImage m_result;
};

#endif // IMAGEPROVIDER_H