/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2016 Malte Brandy <malte.brandy@maralorn.de>   *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#pragma once

#include <QtCore/QObject>

class ActivityDetector : public QObject
{
    Q_OBJECT
public slots:
    void setEnabled(bool enabled);

signals:
    void triggered();

private:
    bool m_enabled = false;

    bool eventFilter(QObject* obj, QEvent* ev) override;
};
