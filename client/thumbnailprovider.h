/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2016 Felix Rohrbach <kde@fxrh.de>              *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#pragma once

#include <QtQuick/QQuickAsyncImageProvider>

class TimelineWidget;

template <bool Avatar>
class ImageProviderTemplate : public QQuickAsyncImageProvider {
public:
    explicit ImageProviderTemplate(TimelineWidget* parent) : timeline(parent) {}

    QQuickImageResponse* requestImageResponse(
        const QString& id, const QSize& requestedSize) override;

private:
    const TimelineWidget* const timeline;
    Q_DISABLE_COPY(ImageProviderTemplate)
};

using AvatarProvider = ImageProviderTemplate<true>;
using ThumbnailProvider = ImageProviderTemplate<false>;
