/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2016 Felix Rohrbach <kde@fxrh.de>              *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#include "thumbnailprovider.h"

#include "timelinewidget.h"
#include "quaternionroom.h"
#include "logging_categories.h"

#include <Quotient/user.h>
#include <Quotient/jobs/mediathumbnailjob.h>

#include <QtCore/QCoreApplication> // for qApp
#include <QtCore/QReadWriteLock>
#include <QtCore/QThread>

using Quotient::BaseJob;

inline int checkDimension(int d)
{
    // Emulate ushort overflow if the value is -1 - may cause issues when
    // screen resolution becomes 100K+ each dimension :-D
    return d >= 0 ? d : std::numeric_limits<ushort>::max();
}

inline QDebug operator<<(QDebug dbg, const auto&& size)
    requires std::is_same_v<std::decay_t<decltype(size)>, QSize>
{
    QDebugStateSaver _(dbg);
    return dbg.nospace() << size.width() << 'x' << size.height();
}

class AbstractThumbnailResponse : public QQuickImageResponse {
    Q_OBJECT
public:
    AbstractThumbnailResponse(const TimelineWidget* timeline, QString id,
                              QSize size)
        : timeline(timeline)
        , mediaId(std::move(id))
        , requestedSize(
              { checkDimension(size.width()), checkDimension(size.height()) })
    {
        qCDebug(THUMBNAILS).noquote()
            << mediaId << '@' << requestedSize << "requested";
        if (mediaId.isEmpty() || requestedSize.isEmpty()) {
            qCDebug(THUMBNAILS) << "Returning an empty thumbnail";
            image = { requestedSize, QImage::Format_Invalid };
            emit finished();
            return;
        }
        errorStr = tr("Image request is pending");
        // Start a request on the main thread, concluding the initialisation
        moveToThread(qApp->thread());
        QMetaObject::invokeMethod(this,
                                  &AbstractThumbnailResponse::startRequest);
        // From this point, access to `image` and `errorStr` must be guarded
        // by `lock`
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

    const TimelineWidget* const timeline;
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

namespace {
const auto NoConnectionError =
    AbstractThumbnailResponse::tr("No connection to perform image request");
}

class ThumbnailResponse : public AbstractThumbnailResponse {
    Q_OBJECT
public:
    using AbstractThumbnailResponse::AbstractThumbnailResponse;
    ~ThumbnailResponse() override = default;

private slots:
    void startRequest() override
    {
        Q_ASSERT(QThread::currentThread() == qApp->thread());

        const auto* currentRoom = timeline->currentRoom();
        if (!currentRoom) {
            finish({}, NoConnectionError);
            return;
        }

        job = currentRoom->connection()->getThumbnail(mediaId, requestedSize);

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
    void startRequest() override
    {
        Q_ASSERT(QThread::currentThread() == qApp->thread());

        Quotient::Room* currentRoom = timeline->currentRoom();
        if (!currentRoom) {
            finish({}, NoConnectionError);
            return;
        }

        // NB: both Room:avatar() and User::avatar() invocations return an image
        // available right now and, if needed, request one with the better
        // resolution asynchronously. To get this better resolution image,
        // Avatar elements in QML should call Avatar.reload() in response to
        // Room::avatarChanged() and Room::memberAvatarChanged() (sic!)
        // respectively.
        const auto& w = requestedSize.width();
        const auto& h = requestedSize.height();
        if (mediaId.startsWith(u'!')) {
            if (mediaId != currentRoom->id()) {
                currentRoom = currentRoom->connection()->room(mediaId);
                Q_ASSERT(currentRoom != nullptr);
            }
            // As of libQuotient 0.8, Room::avatar() is the only call in the
            // Room::avatar*() family that substitutes the counterpart's
            // avatar for a direct chat avatar.
            prepareResult(currentRoom->avatar(w, h));
            return;
        }

        auto* user = currentRoom->user(mediaId);
        Q_ASSERT(user != nullptr);
        prepareResult(user->avatar(w, h, currentRoom));
    }

    void prepareResult(const QImage& avatar)
    {
        qCDebug(THUMBNAILS).noquote() << "Returning avatar for" << mediaId
                                      << "with size:" << avatar.size();
        finish(avatar);
    }
};

#include "thumbnailprovider.moc" // Because we define a Q_OBJECT in the cpp file

template <class ResponseT>
class ImageProviderTemplate : public QQuickAsyncImageProvider {
public:
    explicit ImageProviderTemplate(TimelineWidget* parent) : timeline(parent) {}

private:
    QQuickImageResponse* requestImageResponse(const QString& id,
                                              const QSize& requestedSize) override
    {
        return new ResponseT(timeline, id, requestedSize);
    }

    const TimelineWidget* const timeline;
    Q_DISABLE_COPY(ImageProviderTemplate)
};

QQuickAsyncImageProvider* makeAvatarProvider(TimelineWidget* parent)
{
    return new ImageProviderTemplate<AvatarResponse>(parent);
}

QQuickAsyncImageProvider* makeThumbnailProvider(TimelineWidget* parent)
{
    return new ImageProviderTemplate<ThumbnailResponse>(parent);
}
