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

#ifndef QMATRIXCLIENT_MEDIATHUMBNAILJOB_H
#define QMATRIXCLIENT_MEDIATHUMBNAILJOB_H

#include "basejob.h"

#include <QtGui/QPixmap>

namespace QMatrixClient
{
    enum class ThumbnailType {Crop, Scale};

    class MediaThumbnailJob: public BaseJob
    {
            Q_OBJECT
        public:
            MediaThumbnailJob(ConnectionData* data, QUrl url, int requestedWidth, int requestedHeight,
                              ThumbnailType thumbnailType=ThumbnailType::Scale);
            virtual ~MediaThumbnailJob();

            QPixmap thumbnail();

        protected:
            QString apiPath() override;
            QUrlQuery query() override;

        protected slots:
            void gotReply() override;

        private:
            class Private;
            Private* d;
    };
}

#endif // QMATRIXCLIENT_MEDIATHUMBNAILJOB_H