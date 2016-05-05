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

#ifndef IMAGEPROVIDER_H
#define IMAGEPROVIDER_H

#include <QtQuick/QQuickImageProvider>
#include <QtCore/QThread>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>

#include <lib/jobs/basejob.h>
#include "quaternionconnection.h"

struct ImageProviderData
{
    QPixmap* pixmap;
    QWaitCondition* condition;
    QSize requestedSize;
};

class ImageProvider: public QObject, public QQuickImageProvider
{
        Q_OBJECT
    public:
        ImageProvider(QMatrixClient::Connection* connection);

        QPixmap requestPixmap(const QString& id, QSize* size, const QSize& requestedSize);

        void setConnection(QMatrixClient::Connection* connection);

    private slots:
        void gotImage(QMatrixClient::BaseJob* job);

    private:
        Q_INVOKABLE void doRequest(QString id, QSize requestedSize, QPixmap* pixmap, QWaitCondition* condition);

        QMatrixClient::Connection* m_connection;
        QHash<QMatrixClient::MediaThumbnailJob*, ImageProviderData> m_callmap;
        QMutex m_mutex;
};

Q_DECLARE_METATYPE(QPixmap*)
Q_DECLARE_METATYPE(QWaitCondition*)

#endif // IMAGEPROVIDER_H
