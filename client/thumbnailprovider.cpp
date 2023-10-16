/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2016 Felix Rohrbach <kde@fxrh.de>              *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#include "thumbnailprovider.h"

#include "logging_categories.h"

#include <Quotient/connection.h>
#include <Quotient/user.h>
#include <Quotient/jobs/mediathumbnailjob.h>

#include <QtCore/QReadWriteLock>
#include <QtCore/QThread>
#include <QtCore/QDebug>

using Quotient::Connection;
using Quotient::BaseJob;

inline QDebug operator<<(QDebug dbg, const auto&& size)
    requires std::is_same_v<std::decay_t<decltype(size)>, QSize>
{
    QDebugStateSaver _(dbg);
    return dbg.nospace() << size.width() << 'x' << size.height();
}

class AbstractThumbnailResponse : public QQuickImageResponse {
    Q_OBJECT
public:
    AbstractThumbnailResponse(Connection* c, QString id, QSize size)
        : connection(c), mediaId(std::move(id)), requestedSize(size)
    {
        qCDebug(THUMBNAILS).noquote()
            << mediaId << '@' << requestedSize << "requested";
        if (!c)
            errorStr = tr("No connection to perform image request");
        else if (mediaId.isEmpty() || size.isEmpty()) {
            qCDebug(THUMBNAILS) << "Returning an empty thumbnail";
            image = { requestedSize, QImage::Format_Invalid };
        } else if (!mediaId.startsWith('!') && mediaId.count('/') != 1)
            errorStr =
                tr("Media id '%1' doesn't follow server/mediaId pattern")
                        .arg(mediaId);
        else {
            errorStr = tr("Image request is pending");
            // Start a request on the main thread, concluding the initialisation
            moveToThread(connection->thread());
            QMetaObject::invokeMethod(this,
                                      &AbstractThumbnailResponse::startRequest);
            // From this point, access to `image` and `errorStr` must be guarded
            // by `lock`
            return;
        }
        emit finished();
    }

protected:
    // The two below run in the main thread, not QML thread
    virtual void startRequest() = 0;
    virtual void doCancel() {}

    void finish(const QImage& result, const QString& error = {})
    {
        {
            QWriteLocker _(&lock);
            image = result;
            errorStr = error;
        }
        emit finished();
    }

    Connection* const connection = nullptr;
    const QString mediaId{};
    const QSize requestedSize{};

private:
    QImage image{};
    QString errorStr{};
    mutable QReadWriteLock lock{}; // Guards ONLY these two above

    // The following overrides run in QML thread

    QQuickTextureFactory* textureFactory() const override
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
        QMetaObject::invokeMethod(this, &AbstractThumbnailResponse::doCancel);
    }
};

class ThumbnailResponse : public AbstractThumbnailResponse {
    Q_OBJECT
public:
    using AbstractThumbnailResponse::AbstractThumbnailResponse;

    ~ThumbnailResponse() override = default;

private slots:
    void startRequest() override
    {
        Q_ASSERT(QThread::currentThread() == connection->thread());

        job = connection->getThumbnail(mediaId, requestedSize);
        // Connect to any possible outcome including abandonment
        // to make sure the QML thread is not left stuck forever.
        connect(job, &BaseJob::finished, this, [this] {
            Q_ASSERT(job->error() != BaseJob::Pending);
            if (job->error() == BaseJob::Success) {
                qCDebug(THUMBNAILS).noquote()
                    << "Thumbnail for" << mediaId
                    << "ready, actual size:" << job->thumbnail().size();
                finish(job->thumbnail());
            } else if (job->error() == BaseJob::Abandoned) {
                qCDebug(THUMBNAILS) << "Request cancelled for" << mediaId;
                finish({}, tr("Image request has been cancelled"));
            } else {
                qCWarning(THUMBNAILS).nospace()
                    << "No valid thumbnail for" << mediaId << ": "
                    << job->errorString();
                finish({}, job->errorString());
            }
            job = nullptr;
        });
    }

    void doCancel() override
    {
        if (job) {
            Q_ASSERT(QThread::currentThread() == job->thread());
            job->abandon();
        }
    }

private:
    QPointer<Quotient::MediaThumbnailJob> job = nullptr;
};

class AvatarResponse : public AbstractThumbnailResponse {
    Q_OBJECT
public:
    using AbstractThumbnailResponse::AbstractThumbnailResponse;

private:
    Quotient::Room* room = nullptr;

    void startRequest() override
    {
        Q_ASSERT(QThread::currentThread() == connection->thread());
        const auto parts = mediaId.split(u'/');
        if (parts.size() > 2) { // Not tr() because it's internal error
            qCCritical(THUMBNAILS) << "Avatar reference '" << mediaId
                                   << "' must look like 'roomid[/userid]'";
            Q_ASSERT(parts.size() <= 2);
        }
        room = connection->room(parts.front());
        Q_ASSERT(room != nullptr);

        // NB: both Room:avatar() and User::avatar() invocations return an image
        // available right now and, if needed, request one with the better
        // resolution asynchronously. To get this better resolution image,
        // Avatar elements in QML should call Avatar.reload() in response to
        // Room::avatarChanged() and Room::memberAvatarChanged() (sic!)
        // respectively.
        const auto& w = requestedSize.width();
        const auto& h = requestedSize.height();
        if (parts.size() == 1) {
            // As of libQuotient 0.8, Room::avatar() is the only call in the
            // Room::avatar*() family that substitutes the counterpart's
            // avatar for a direct chat avatar.
            prepareResult(room->avatar(w, h));
            return;
        }

        auto* user = room->user(parts.back());
        Q_ASSERT(user != nullptr);
        prepareResult(user->avatar(w, h, room));
    }

    void prepareResult(const QImage& avatar)
    {
        qCDebug(THUMBNAILS).noquote() << "Returning avatar for" << mediaId
                                      << "with size:" << avatar.size();
        finish(avatar);
    }
};

#include "thumbnailprovider.moc" // Because we define a Q_OBJECT in the cpp file

QQuickImageResponse* ThumbnailProvider::requestImageResponse(
        const QString& id, const QSize& requestedSize)
{
    auto size = requestedSize;
    // Force integer overflow if the value is -1 - may cause issues when
    // screens resolution becomes 100K+ each dimension :-D
    if (size.width() == -1)
        size.setWidth(ushort(-1));
    if (size.height() == -1)
        size.setHeight(ushort(-1));
    auto* const c = m_connection.loadRelaxed();
    if (id.startsWith(u'!'))
        return new AvatarResponse(c, id, size);
    return new ThumbnailResponse(c, id, size);
}
