CMAKE_MINIMUM_REQUIRED(VERSION 3.16)
if (POLICY CMP0092)
cmake_policy(SET CMP0092 NEW)
endif()

set(IDENTIFIER "io.github.quotient_im.Quaternion")
set(COPYRIGHT "Copyright © The Quotient Project contributors")

project(quaternion VERSION 0.0.96.82 LANGUAGES CXX)

if(UNIX AND NOT APPLE)
    set(LINUX 1)
endif(UNIX AND NOT APPLE)

include(CheckCXXCompilerFlag)

if (WIN32)
    if (NOT CMAKE_INSTALL_BINDIR)
        set(CMAKE_INSTALL_BINDIR ".")
    endif()
else()
    include(GNUInstallDirs)
    include(cmake/ECMInstallIcons.cmake)
endif(WIN32)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Debug' as none was specified")
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build" FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
endif()

# Setup command line parameters for the compiler and linker
if (MSVC)
    add_compile_options(/EHsc /W4
        /wd4100 /wd4127 /wd4242 /wd4244 /wd4245 /wd4267 /wd4365 /wd4456 /wd4459
        /wd4464 /wd4505 /wd4514 /wd4571 /wd4619 /wd4623 /wd4625 /wd4626 /wd4706
        /wd4710 /wd4774 /wd4820 /wd4946 /wd5026 /wd5027)
else()
    foreach (FLAG all pedantic extra error=return-void) # Switch these on
        CHECK_CXX_COMPILER_FLAG("-W${FLAG}" W${FLAG}_SUPPORTED)
        mark_as_advanced(W${FLAG}_SUPPORTED)
        if (W${FLAG}_SUPPORTED AND NOT CMAKE_CXX_FLAGS MATCHES "W(no-)?${FLAG}($| )")
            add_compile_options(-W${FLAG})
        endif ()
    endforeach ()
    foreach (FLAG unused-parameter subobject-linkage) # Switch these off
        CHECK_CXX_COMPILER_FLAG("-Wno-${FLAG}" Wno-${FLAG}_SUPPORTED)
        mark_as_advanced(Wno-${FLAG}_SUPPORTED)
        if (Wno-${FLAG}_SUPPORTED AND NOT CMAKE_CXX_FLAGS MATCHES "W(no-)?${FLAG}($| )")
            add_compile_options(-Wno-${FLAG})
        endif ()
    endforeach ()
endif()

set(QtMinVersion "6.4")
set(QUOTIENT "QuotientQt6")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15")
string(REGEX REPLACE "^(.).*" "Qt\\1" Qt ${QtMinVersion}) # makes "Qt<MajorVersionDigit>"

# Find the libraries
find_package(${Qt} ${QtMinVersion} REQUIRED
    Widgets Network Quick Gui Multimedia QuickControls2 QuickWidgets
    LinguistTools)
# ${Qt}_Prefix is only used to show Qt path in message()
# ${Qt}_BinDir is where all the *deployqt tools reside
if (QT_QMAKE_EXECUTABLE)
    get_filename_component(${Qt}_BinDir "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    get_filename_component(${Qt}_Prefix "${${Qt}_BinDir}/.." ABSOLUTE)
else()
    get_filename_component(${Qt}_BinDir "${${Qt}_DIR}/../../../bin" ABSOLUTE)
    get_filename_component(${Qt}_Prefix "${${Qt}_DIR}/../../../.." ABSOLUTE)
endif()

if(WIN32)
    enable_language(RC)
    include(CMakeDetermineRCCompiler)

    if(MINGW)
        set(CMAKE_RC_COMPILER_INIT windres)
        set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> <FLAGS> -O coff <DEFINES> -I${CMAKE_CURRENT_BINARY_DIR} -i <SOURCE> -o <OBJECT>")
    endif()
endif()

set(QUOTIENT_FORCE_NAMESPACED_INCLUDES 1)
if ((NOT DEFINED USE_INTREE_LIBQMC OR USE_INTREE_LIBQMC)
        AND EXISTS ${PROJECT_SOURCE_DIR}/lib/Quotient/util.h)
    add_subdirectory(lib)
    if (NOT DEFINED USE_INTREE_LIBQMC)
        set (USE_INTREE_LIBQMC 1)
    endif ()
endif ()
if (NOT USE_INTREE_LIBQMC)
    find_package(${QUOTIENT} 0.9.2 REQUIRED)
    if (NOT ${QUOTIENT}_FOUND)
        message(WARNING "libQuotient not found; configuration will most likely fail.")
        message(WARNING "Make sure you have installed libQuotient development files")
        message(WARNING "as a package or checked out the library sources in lib/.")
        message(WARNING "See also BUILDING.md")
    endif ()
endif ()

message(STATUS)
message(STATUS "== Quaternion ${quaternion_VERSION} configuration summary ==")
message(STATUS)
if (CMAKE_BUILD_TYPE)
    message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
endif(CMAKE_BUILD_TYPE)
message(STATUS "Quaternion install prefix: ${CMAKE_INSTALL_PREFIX}")
# Get Git info if possible
find_package(Git)
if(GIT_FOUND)
    execute_process(COMMAND
        "${GIT_EXECUTABLE}" rev-parse -q HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_SHA1
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    message(STATUS "Git SHA1: ${GIT_SHA1}")
endif()
message(STATUS "Using compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Using Qt ${${Qt}_VERSION} at ${${Qt}_Prefix}")
if (USE_INTREE_LIBQMC)
    message(STATUS "Using in-tree libQuotient")
    if (GIT_FOUND)
        execute_process(COMMAND
            "${GIT_EXECUTABLE}" rev-parse -q HEAD
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib
            OUTPUT_VARIABLE LIB_GIT_SHA1
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        message(STATUS "  Library git SHA1: ${LIB_GIT_SHA1}")
    endif (GIT_FOUND)
else ()
    message(STATUS "Using libQuotient ${${QUOTIENT}_VERSION} at ${${QUOTIENT}_DIR}")
endif ()
message(STATUS)

# Windows, this is a GUI executable; OSX, make a bundle
add_executable(${PROJECT_NAME} WIN32 MACOSX_BUNDLE)

# Set up source files
target_sources(${PROJECT_NAME} PRIVATE
    client/quaternionroom.cpp client/quaternionroom.h
    client/htmlfilter.cpp client/htmlfilter.h
    client/activitydetector.cpp client/activitydetector.h
    client/dialog.cpp client/dialog.h
    client/logindialog.cpp client/logindialog.h
    client/networkconfigdialog.cpp client/networkconfigdialog.h
    client/roomdialogs.cpp client/roomdialogs.h
    client/dockmodemenu.cpp client/dockmodemenu.h
    client/mainwindow.cpp client/mainwindow.h
    client/roomlistdock.cpp client/roomlistdock.h
    client/userlistdock.cpp client/userlistdock.h
    client/accountselector.cpp client/accountselector.h
    client/kchatedit.cpp client/kchatedit.h
    client/chatedit.cpp client/chatedit.h
    client/timelinewidget.cpp client/timelinewidget.h
    client/chatroomwidget.cpp client/chatroomwidget.h
    client/systemtrayicon.cpp client/systemtrayicon.h
    client/profiledialog.cpp client/profiledialog.h
    client/models/messageeventmodel.cpp client/models/messageeventmodel.h
    client/models/userlistmodel.cpp client/models/userlistmodel.h
    client/models/roomlistmodel.cpp client/models/roomlistmodel.h
    client/models/abstractroomordering.cpp client/models/abstractroomordering.h
    client/models/orderbytag.cpp client/models/orderbytag.h
    client/desktop_integration.h client/logging_categories.h
    client/main.cpp
    client/resources.qrc
)

# quaternion_en.ts is updated explicitly by building trbase target,
# while all other translation files are created and updated externally at
# Lokalise.co
set(quaternion_en_TS client/translations/quaternion_en.ts)
QT_CREATE_TRANSLATION(quaternion_QM ${quaternion_en_TS}
                      client/ client/qml/ OPTIONS -no-obsolete)
add_custom_target(trbase DEPENDS ${quaternion_en_TS})

QT_ADD_TRANSLATION(quaternion_QM
    client/translations/quaternion_en_GB.ts
    client/translations/quaternion_de.ts
    client/translations/quaternion_pl.ts
    client/translations/quaternion_ru.ts
    client/translations/quaternion_es.ts
)
target_sources(${PROJECT_NAME} PRIVATE ${quaternion_QM})

if(WIN32)
    set(quaternion_WINRC quaternion_win32.rc)
    set_property(SOURCE ${quaternion_WINRC} APPEND PROPERTY
        OBJECT_DEPENDS ${PROJECT_SOURCE_DIR}/icons/quaternion.ico
    )
    target_sources(${PROJECT_NAME} PRIVATE ${quaternion_WINRC})
endif()

if(APPLE)
    MESSAGE(STATUS "CMAKE_OSX_DEPLOYMENT_TARGET: " ${CMAKE_OSX_DEPLOYMENT_TARGET})

    set(MACOSX_BUNDLE_GUI_IDENTIFIER ${IDENTIFIER})
    set(MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME})

    set(MACOSX_BUNDLE_COPYRIGHT ${COPYRIGHT})

    set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${quaternion_VERSION})
    set(MACOSX_BUNDLE_BUNDLE_VERSION ${quaternion_VERSION})

    set(ICON_NAME "quaternion.icns")
    set(${PROJECT_NAME}_MAC_ICON "${PROJECT_SOURCE_DIR}/icons/${ICON_NAME}")
    set(MACOSX_BUNDLE_ICON_FILE ${ICON_NAME})
    set_property(SOURCE "${${PROJECT_NAME}_MAC_ICON}" PROPERTY
        MACOSX_PACKAGE_LOCATION Resources)
    target_sources(${PROJECT_NAME} PRIVATE ${${PROJECT_NAME}_MAC_ICON})
endif(APPLE)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    GIT_SHA1="${GIT_SHA1}" LIB_GIT_SHA1="${LIB_GIT_SHA1}")
target_compile_definitions(${PROJECT_NAME} PRIVATE QT_NO_JAVA_STYLE_ITERATORS)
if (NOT CMAKE_CXX_COMPILER_ID STREQUAL GNU) # https://bugzilla.redhat.com/show_bug.cgi?id=1721553
    target_precompile_headers(${PROJECT_NAME} PRIVATE <Quotient/room.h>)
endif ()
target_link_libraries(${PROJECT_NAME} ${QUOTIENT}
    ${Qt}::Widgets ${Qt}::Quick ${Qt}::Qml ${Qt}::Gui ${Qt}::Network ${Qt}::QuickControls2 ${Qt}::QuickWidgets)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 23
    CXX_EXTENSIONS OFF
    VISIBILITY_INLINES_HIDDEN ON
    CXX_VISIBILITY_PRESET hidden
)
# macOS specific config for bundling
if (APPLE)
    set_property(TARGET ${PROJECT_NAME} PROPERTY MACOSX_BUNDLE_INFO_PLIST
                 "${PROJECT_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in")
endif()

# Installation

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        BUNDLE DESTINATION .
)
if(LINUX)
    install(FILES linux/${IDENTIFIER}.desktop
            DESTINATION  ${CMAKE_INSTALL_DATADIR}/applications
            )
    install(FILES linux/${IDENTIFIER}.appdata.xml
            DESTINATION  ${CMAKE_INSTALL_DATADIR}/metainfo
            )
    install(FILES ${quaternion_QM}
            DESTINATION ${CMAKE_INSTALL_DATADIR}/Quotient/quaternion/translations
            )
    file(GLOB quaternion_icons icons/quaternion/*-apps-quaternion.png)
    ecm_install_icons(ICONS ${quaternion_icons} icons/quaternion/sources/sc-apps-quaternion.svg
                      DESTINATION ${CMAKE_INSTALL_DATADIR}/icons
                      )
endif(LINUX)

set(QML_DIR ${PROJECT_SOURCE_DIR}/client/qml)
if (NOT DEPLOY_VERBOSITY)
    set(DEPLOY_VERBOSITY 1) # The default for *deployqt tools, out of 0..3
endif()
if(WIN32)
    install(CODE "
        message(STATUS \"Running windeployqt at \${CMAKE_INSTALL_PREFIX}\${CMAKE_INSTALL_BINDIR}\")
        execute_process(
            COMMAND \"${${Qt}_BinDir}/windeployqt\" --verbose ${DEPLOY_VERBOSITY}
                --no-multimediaquick --no-test --qmldir \"${QML_DIR}\"
                \${CMAKE_INSTALL_PREFIX}\${CMAKE_INSTALL_BINDIR}
            RESULT_VARIABLE WDQ_RETVAL
        )
        if (WDQ_RETVAL)
            message(FATAL_ERROR \"windeployqt returned \${WDQ_RETVAL} - check messages above\")
        else()
            message(STATUS \"Quaternion and its dependencies have been deployed to \${CMAKE_INSTALL_PREFIX}.\")
        endif()
    ")
    install(FILES ${quaternion_QM}
            DESTINATION ${CMAKE_INSTALL_BINDIR}/translations
           )
endif(WIN32)

# Packaging

if(APPLE)
    execute_process(
        COMMAND "${${Qt}_BinDir}/qmake" -query QT_INSTALL_TRANSLATIONS
        OUTPUT_VARIABLE _qt_translations_dir
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY
    )
    set(MACDEPLOYQT_ARGS ${PROJECT_NAME}.app -dmg -qmldir="${QML_DIR}" -verbose=${DEPLOY_VERBOSITY})
    add_custom_target(image
        COMMAND mkdir ${PROJECT_NAME}.app/Contents/Translations
        COMMAND install "${_qt_translations_dir}/qtbase_*.qm"
                        "${_qt_translations_dir}/qtdeclarative_*.qm"
                        "${_qt_translations_dir}/qtmultimedia_*.qm"
                        ${PROJECT_NAME}.app/Contents/Translations
        COMMAND "${${Qt}_BinDir}/macdeployqt" ${MACDEPLOYQT_ARGS}
        DEPENDS ${PROJECT_NAME}
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
        COMMENT "Running ${MACDEPLOYQT} with args: ${MACDEPLOYQT_ARGS}"
    )
endif(APPLE)
