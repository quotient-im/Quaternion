/**************************************************************************
 *                                                                        *
 * SPDX-FileCopyrightText: 2015 Felix Rohrbach <kde@fxrh.de>              *
 *                                                                        *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *                                                                        *
 **************************************************************************/

#include "desktop_integration.h"
#include "logging_categories.h"
#include "mainwindow.h"

#include <Quotient/networksettings.h>
#include <Quotient/connection.h>

#include <QtQuickControls2/QQuickStyle>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleFactory>

#include <QtCore/QCommandLineParser>
#include <QtCore/QLibraryInfo>
#include <QtCore/QStandardPaths>
#include <QtCore/QTranslator>

using namespace Qt::StringLiterals;

namespace {
inline void loadTranslations()
{
// Extract a number from another macro and turn it to a const char[]
#define ITOA(i) #i
    static const auto translationConfigs = std::to_array<std::pair<QStringList, QString>>(
        { { { u"qt"_s, u"qtbase"_s, u"qtnetwork"_s, u"qtdeclarative"_s, u"qtmultimedia"_s,
              u"qtquickcontrols"_s, u"qtquickcontrols2"_s,
              // QtKeychain tries to install its translations to Qt's path;
              // try to look there, just in case (see also below)
              u"qtkeychain"_s },
            QLibraryInfo::path(QLibraryInfo::TranslationsPath) },
          { { u"qtkeychain"_s },
            QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                   u"qt" ITOA(QT_VERSION_MAJOR) "keychain/translations"_s,
                                   QStandardPaths::LocateDirectory) },
          { { u"qt"_s, u"qtkeychain"_s, u"quotient"_s, AppName },
            QStandardPaths::locate(QStandardPaths::AppLocalDataLocation, u"translations"_s,
                                   QStandardPaths::LocateDirectory) } });
#undef ITOA

    for (const auto& [configNames, configPath] : translationConfigs)
        for (const auto& configName : configNames) {
            auto translator = std::make_unique<QTranslator>();
            // Check the current directory then configPath
            if (translator->load(QLocale(), configName, u"_"_s)
                || translator->load(QLocale(), configName, u"_"_s, configPath)) {
                auto path = translator->filePath();
                if (QApplication::installTranslator(translator.get())) {
                    qCDebug(MAIN).noquote() << "Loaded translations from" << path;
                    translator.release()->setParent(qApp); // Change pointer ownership
                } else
                    qCWarning(MAIN).noquote() << "Failed to load translations from" << path;
            } else
                qCDebug(MAIN) << "No translations for" << configName << "at" << configPath;
        }
}
}

int main( int argc, char* argv[] )
{
    QApplication::setOrganizationName(u"Quotient"_s);
    QApplication::setApplicationName(AppName);
    QApplication::setApplicationDisplayName(u"Quaternion"_s);
    QApplication::setApplicationVersion(u"0.0.97 beta2 (+git)"_s);
    QApplication::setDesktopFileName(AppId);

    using Quotient::Settings;
    Settings::setLegacyNames(u"QMatrixClient"_s, u"quaternion"_s);
    Settings settings;

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
        QIcon::setFallbackThemeName("breeze");
    } else
#endif
    {
        QQuickStyle::setFallbackStyle(u"Fusion"_s); // Looks better on desktops
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

        qCInfo(MAIN) << "Using application font:" << font.toString();
        QApplication::setFont(font);
    }

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
        QLocale::setDefault(QLocale(overrideLocale));
        qCInfo(MAIN) << "Using locale" << QLocale().name();
    }

    loadTranslations();

    Quotient::NetworkSettings().setupApplicationProxy();
    Quotient::Connection::setEncryptionDefault(true);

    MainWindow window;
    if (parser.isSet(hideMainWindow)) {
        qCDebug(MAIN) << "--- Hide time!";
        window.hide();
    }
    else {
        qCDebug(MAIN) << "--- Show time!";
        window.show();
    }

    return QApplication::exec();
}

