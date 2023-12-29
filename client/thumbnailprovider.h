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

QQuickAsyncImageProvider* makeAvatarProvider(TimelineWidget* parent);
QQuickAsyncImageProvider* makeThumbnailProvider(TimelineWidget* parent);
