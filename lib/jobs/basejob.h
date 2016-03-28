/******************************************************************************
 * Copyright (C) 2015 Felix Rohrbach <kde@fxrh.de>
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

#ifndef QMATRIXCLIENT_BASEJOB_H
#define QMATRIXCLIENT_BASEJOB_H

#ifdef USING_SYSTEM_KCOREADDONS
#include <KCoreAddons/KJob>
#else
#include "kjob.h"
#endif // KCOREADDONS_FOUND

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QUrlQuery>
#include <QtNetwork/QNetworkReply>

namespace QMatrixClient
{
    class ConnectionData;

    enum class JobHttpType { GetJob, PutJob, PostJob };
    
    class BaseJob: public KJob
    {
            Q_OBJECT
        public:
            BaseJob(ConnectionData* connection, JobHttpType type, bool needsToken=true);
            virtual ~BaseJob();

            void start() override;

            enum ErrorCode { NetworkError = KJob::UserDefinedError, JsonParseError, UserDefinedError };
            
        protected:
            ConnectionData* connection() const;

            // to implement
            virtual QString apiPath()=0;
            virtual QUrlQuery query();
            virtual QJsonObject data();
            virtual void parseJson(const QJsonDocument& data);
            
            void fail( int errorCode, QString errorString );
            QNetworkReply* networkReply() const;

            
        protected slots:
            virtual void gotReply();
            void timeout();

            //void networkError(QNetworkReply::NetworkError code);


        private:
            class Private;
            Private* d;
    };
}

#endif // QMATRIXCLIENT_BASEJOB_H
