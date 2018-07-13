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
#include <QtCore/QTranslator>
#include <QtCore/QLibraryInfo>
#include <QtCore/QCommandLineParser>
#include <QtCore/QDebug>

#include "networksettings.h"
#include "mainwindow.h"
#include "activitydetector.h"
#include <settings.h>

int main( int argc, char* argv[] )
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName("QMatrixClient");
    QApplication::setApplicationName("quaternion");
    QApplication::setApplicationDisplayName("Quaternion");
    QApplication::setApplicationVersion("0.0.9.3");

    QMatrixClient::Settings::setLegacyNames("Quaternion", "quaternion");

    // We should not need to do the following, as quitOnLastWindowClosed is
    // set to "true" by default; might be a bug, see
    // https://forum.qt.io/topic/71112/application-does-not-quit
    QObject::connect(&app, &QApplication::lastWindowClosed, []{
        qDebug() << "Last window closed!";
        QApplication::postEvent(qApp, new QEvent(QEvent::Quit));
    });

    QTranslator qtTranslator;
    qtTranslator.load(QLocale(), "qt", "_",
                      QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    app.installTranslator(&qtTranslator);

    QTranslator appTranslator;
    appTranslator.load(QLocale(), "quaternion", ".", "i18n");
    app.installTranslator(&appTranslator);

    QCommandLineParser parser;
    parser.setApplicationDescription(QApplication::translate("main", "An IM client for the Matrix protocol"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption debug("debug", QApplication::translate("main", "Display debug information"));
    parser.addOption(debug);

    parser.process(app);
    bool debugEnabled = parser.isSet(debug);
    qDebug() << "Debug: " << debugEnabled;

#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
    app.setAttribute(Qt::AA_DisableWindowContextHelpButton);
#endif

    QMatrixClient::NetworkSettings().setupApplicationProxy();

    MainWindow window;
    if( debugEnabled )
        window.enableDebug();

    ActivityDetector ad(app, window); Q_UNUSED(ad);
    qDebug() << "--- Show time!";
    window.show();

    return app.exec();
}

