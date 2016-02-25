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

#include "user.h"

#include "connection.h"
#include "events/event.h"
#include "events/roommemberevent.h"
#include "jobs/mediathumbnailjob.h"

#include <QtCore/QTimer>
#include <QtCore/QPair>

using namespace QMatrixClient;

class User::Private: public QObject
{
    public:
        User* q;
        QString userId;
        QString name;
        QUrl avatarUrl;
        Connection* connection;

        QPixmap avatar;
        int requestedWidth;
        int requestedHeight;
        bool avatarValid;
        bool avatarOngoingRequest;
        QHash<QPair<int,int>,QPixmap> scaledMap;

        void requestAvatar();
    public slots:
        void gotAvatar(KJob* job);
};

User::User(QString userId, Connection* connection)
    : d(new Private)
{
    d->q = this;
    d->connection = connection;
    d->userId = userId;
    d->avatarValid = false;
    d->avatarOngoingRequest = false;
}

User::~User()
{
    delete d;
}

QString User::id() const
{
    return d->userId;
}

QString User::name() const
{
    return d->name;
}

QString User::displayname() const
{
    if( !d->name.isEmpty() )
        return d->name;
    return d->userId;
}

QPixmap User::avatar(int width, int height)
{
    if( !d->avatarValid
        || width > d->requestedWidth
        || height > d->requestedHeight )
    {
        if( !d->avatarOngoingRequest && d->avatarUrl.isValid() )
        {
            qDebug() << "Get avatar...";
            d->requestedWidth = width;
            d->requestedHeight = height;
            d->avatarOngoingRequest = true;
            QTimer::singleShot(0, this, SLOT(requestAvatar()));
        }
    }

    if( d->avatar.isNull() )
        return d->avatar;
    QPair<int,int> size(width, height);
    if( !d->scaledMap.contains(size) )
    {
        d->scaledMap.insert(size, d->avatar.scaled(width, height, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    return d->scaledMap.value(size);
}

void User::processEvent(Event* event)
{
    if( event->type() == EventType::RoomMember )
    {
        RoomMemberEvent* e = static_cast<RoomMemberEvent*>(event);
        if( d->name != e->displayName() )
        {
            d->name = e->displayName();
            emit nameChanged();
        }
        if( d->avatarUrl != e->avatarUrl() )
        {
            d->avatarUrl = e->avatarUrl();
            d->avatarValid = false;
        }
    }
}

void User::requestAvatar()
{
    d->requestAvatar();
}

void User::Private::requestAvatar()
{
    MediaThumbnailJob* job =
            connection->getThumbnail(avatarUrl, requestedWidth, requestedHeight);
    connect( job, &MediaThumbnailJob::result, this, &User::Private::gotAvatar );
}

void User::Private::gotAvatar(KJob* job)
{
    avatarOngoingRequest = false;
    avatarValid = true;
    avatar =
        static_cast<MediaThumbnailJob*>(job)->thumbnail()
            .scaled(requestedWidth, requestedHeight,
                    Qt::KeepAspectRatio, Qt::SmoothTransformation);
    scaledMap.clear();
    emit q->avatarChanged(q);
}