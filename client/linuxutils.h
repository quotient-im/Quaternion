/*
 * SPDX-FileCopyrightText: 2017 Elvis Angelaccio <elvis.angelaccio@kde.org>
 * SPDX-FileCopyrightText: 2020 The Quotient project
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QtCore/QFileInfo>

inline bool inFlatpak() {
    return QFileInfo::exists("/.flatpak-info");
}

inline QString appIconName() {
    return inFlatpak() ? "com.github.quaternion" : "quaternion";
}
