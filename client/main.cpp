/**************************************************************************
 *                                                                        *
 * Copyright (C) 2015 Felix Rohrbach <kde@fxrh.de>                        *
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

#include <QtWidgets/QApplication>
#include <QtCore/QCommandLineParser>
#include <QtCore/QDebug>

#include "mainwindow.h"
#include "activitydetector.h"

int main( int argc, char* argv[] )
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName("Quaternion");
    QApplication::setApplicationName("quaternion");
    QApplication::setApplicationDisplayName("Quaternion");
    QApplication::setApplicationVersion("0.0");

    QCommandLineParser parser;
    parser.setApplicationDescription(QApplication::translate("main", "An IM client for the Matrix protocol"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption debug("debug", QApplication::translate("main", "Display debug information"));
    parser.addOption(debug);

    parser.process(app);
    bool debugEnabled = parser.isSet(debug);
    qDebug() << "Debug: " << debugEnabled;

    MainWindow window;
    if( debugEnabled )
        window.enableDebug();
    ActivityDetector ad(app, window); Q_UNUSED(ad);
    window.show();

    return app.exec();
}

