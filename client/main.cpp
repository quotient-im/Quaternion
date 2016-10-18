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
#include <QtCore/QCommandLineOption>
#include <QtCore/QDebug>

#include "mainwindow.h"

class ActivityDetector : public QObject
{
    public:
        ActivityDetector(MainWindow* c): m_mainWindow(c)
        {
            c->setMouseTracking(true);
        };
    protected:
        bool eventFilter(QObject* obj, QEvent* ev)
        {
            switch (ev->type())
            {
            case QEvent::KeyPress:
            case QEvent::FocusIn:
            case QEvent::MouseMove:
            case QEvent::MouseButtonPress:
                m_mainWindow->activity();
            default:;
            }
            return QObject::eventFilter(obj, ev);
        }
    private:
        MainWindow* m_mainWindow;
};


int main( int argc, char* argv[] )
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName("Quaternion");
    QApplication::setApplicationName("quaternion");
    QApplication::setApplicationDisplayName("Quaternion");
    QApplication::setApplicationVersion("0.0");

    QObject::connect(&app, &QApplication::lastWindowClosed, []{
        qDebug() << "Last window closed!";
        // We should not need to do the following, as quitOnLastWindowClosed is set to "true"
        // might be a bug, see https://forum.qt.io/topic/71112/application-does-not-quit
        QApplication::postEvent(qApp, new QEvent(QEvent::Quit));
    });

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
    ActivityDetector ad(&window);
    app.installEventFilter(&ad);
    window.show();

    return app.exec();
}

