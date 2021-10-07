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
#include <QtCore/QLoggingCategory>
#include <QtCore/QStandardPaths>
#include <QtWidgets/QStyleFactory>
#include <QtQuickControls2/QQuickStyle>

#include "networksettings.h"
#include "mainwindow.h"
#include "linuxutils.h"
#include <settings.h>

void loadTranslations(
        std::initializer_list<std::pair<QStringList, QString>> translationConfigs)
{
    for (const auto& [configNames, configPath]: translationConfigs)
        for (const auto& configName: configNames) {
            auto* translator = new QTranslator(qApp);
            bool loaded = false;
            // Check the current directory then configPath
            if (translator->load(QLocale(), configName, "_")
                || translator->load(QLocale(), configName, "_", configPath)) {
                auto path =
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
                    translator->filePath();
#else
                    configPath;
#endif
                if ((loaded = QApplication::installTranslator(translator)))
                    qDebug().noquote() << "Loaded translations from" << path;
                else
                    qWarning().noquote()
                        << "Failed to load translations from" << path;
            } else
                qDebug() << "No translations for" << configName << "at" << configPath;
            if (!loaded)
                delete translator;
        }
}

int main( int argc, char* argv[] )
{
    QApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);
#if defined(Q_OS_LINUX)
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication::setOrganizationName(QStringLiteral("Quotient"));
    QApplication::setApplicationName(QStringLiteral("quaternion"));
    QApplication::setApplicationDisplayName(QStringLiteral("Quaternion"));
    QApplication::setApplicationVersion(QStringLiteral("0.0.95.1"));
    QApplication::setDesktopFileName(
        QStringLiteral("com.github.quaternion.desktop"));

    using Quotient::Settings;
    Settings::setLegacyNames(QStringLiteral("QMatrixClient"),
                             QStringLiteral("quaternion"));
    Settings settings;

    // Qt 5.15 introduces a new way to handle connections in QML and at
    // the same time deprecates the old way. Rather than go to pains of
    // making two different versions of QML code, just disable
    // the deprecation warning as it doesn't help neither users nor developers.
    if (QLibraryInfo::version() >= QVersionNumber(5,15))
        QLoggingCategory::setFilterRules(
            QStringLiteral("qt.qml.connections=false"));

    QApplication app(argc, argv);
#if defined Q_OS_UNIX && !defined Q_OS_MAC
    // #681: When in Flatpak and unless overridden by configuration, set
    // the style to Breeze as it looks much fresher than Fusion that Qt
    // applications default to in Flatpak outside KDE. Although Qt docs
    // recommend to call setStyle() before constructing a QApplication object
    // (to make sure the style's palette is applied?) that doesn't work with
    // Breeze because it seems to make use of platform theme hints, which
    // in turn need a created QApplication object (see #700).
    const auto useBreezeStyle = settings.get("UI/use_breeze_style", inFlatpak());
    if (useBreezeStyle) {
        QApplication::setStyle("Breeze");
        QIcon::setThemeName("breeze");
#    if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        QIcon::setFallbackThemeName("breeze");
#    endif
    } else
#endif
    {
        const auto qqc2styles = QQuickStyle::availableStyles();
        if (qqc2styles.contains("Fusion"))
            QQuickStyle::setFallbackStyle("Fusion"); // Looks better on desktops
//        QQuickStyle::setStyle("Material");
    }

    {
        auto font = QApplication::font();
        if (const auto fontFamily = settings.get<QString>("UI/Fonts/family");
            !fontFamily.isEmpty())
            font.setFamily(fontFamily);

        if (const auto fontPointSize =
                settings.value("UI/Fonts/pointSize").toReal();
            fontPointSize > 0)
            font.setPointSizeF(fontPointSize);

        qDebug() << "Using application font:" << font.toString();
        QApplication::setFont(font);
    }

    // We should not need to do the following, as quitOnLastWindowClosed is
    // set to "true" by default; might be a bug, see
    // https://forum.qt.io/topic/71112/application-does-not-quit
    QObject::connect(&app, &QApplication::lastWindowClosed, &app, [&app]{
        qDebug() << "Last window closed!";
        QApplication::postEvent(&app, new QEvent(QEvent::Quit));
    });

    QCommandLineParser parser;
    parser.setApplicationDescription(QApplication::translate("main",
            "Quaternion - an IM client for the Matrix protocol"));
    parser.addHelpOption();
    parser.addVersionOption();

    QList<QCommandLineOption> options;
    QCommandLineOption locale { QStringLiteral("locale"),
        QApplication::translate("main", "Override locale"),
        QApplication::translate("main", "locale") };
    options.append(locale);
    QCommandLineOption hideMainWindow { QStringLiteral("hide-mainwindow"),
        QApplication::translate("main", "Hide main window on startup") };
    options.append(hideMainWindow);
    // Add more command line options before this line

    if (!parser.addOptions(options))
        Q_ASSERT_X(false, __FUNCTION__,
                   "Command line options are improperly defined, fix the code");
    parser.process(app);

    const auto overrideLocale = parser.value(locale);
    if (!overrideLocale.isEmpty())
    {
        QLocale::setDefault(overrideLocale);
        qInfo() << "Using locale" << QLocale().name();
    }

    loadTranslations(
        { { { "qt", "qtbase", "qtnetwork", "qtdeclarative", "qtmultimedia",
              "qtquickcontrols", "qtquickcontrols2",
              // QtKeychain tries to install its translations to Qt's path;
              // try to look there, just in case (see also below)
              "qtkeychain" },
            QLibraryInfo::location(QLibraryInfo::TranslationsPath) },
          { { "qtkeychain" },
            // Assuming https://github.com/frankosterfeld/qtkeychain/pull/166
            // is accepted and QtKeychain is installed at a default location
            QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                   "qt5keychain/translations",
                                   QStandardPaths::LocateDirectory) },
          { { "qt", "qtkeychain", "quotient", "quaternion" },
            QStandardPaths::locate(QStandardPaths::AppLocalDataLocation,
                                   "translations",
                                   QStandardPaths::LocateDirectory) } });

    Quotient::NetworkSettings().setupApplicationProxy();

    MainWindow window;
    if (parser.isSet(hideMainWindow)) {
        qDebug() << "--- Hide time!";
        window.hide();
    }
    else {
        qDebug() << "--- Show time!";
        window.show();
    }

    return QApplication::exec();
}

