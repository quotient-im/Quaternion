CMAKE_MINIMUM_REQUIRED(VERSION 3.10)
if (POLICY CMP0092)
cmake_policy(SET CMP0092 NEW)
endif()

set(IDENTIFIER "com.github.quaternion")
set(COPYRIGHT "Copyright © 2016-2018 QMatrixClient, 2019 The Quotient Project")

project(quaternion VERSION 0.0.9.5 LANGUAGES CXX)

if(UNIX AND NOT APPLE)
    set(LINUX 1)
endif(UNIX AND NOT APPLE)

include(CheckCXXCompilerFlag)
if (NOT WIN32)
    include(GNUInstallDirs)
    include(cmake/ECMInstallIcons.cmake)
endif(NOT WIN32)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

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
    foreach (FLAG "" all pedantic extra no-unused-parameter)
        CHECK_CXX_COMPILER_FLAG("-W${FLAG}" WARN_${FLAG}_SUPPORTED)
        if ( WARN_${FLAG}_SUPPORTED AND NOT CMAKE_CXX_FLAGS MATCHES "(^| )-W?${FLAG}($| )")
            add_compile_options(-W${FLAG})
        endif ()
    endforeach ()
endif()

# Find the libraries
find_package(Qt5 5.9 REQUIRED Widgets Network Quick Qml Gui LinguistTools Multimedia DBus)
# Qt5_Prefix is only used to show Qt path in message()
# Qt5_BinDir is where all the binary tools for Qt are
if (QT_QMAKE_EXECUTABLE)
    get_filename_component(Qt5_BinDir "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    get_filename_component(Qt5_Prefix "${Qt5_BinDir}/.." ABSOLUTE)
else()
    get_filename_component(Qt5_BinDir "${Qt5_DIR}/../../../bin" ABSOLUTE)
    get_filename_component(Qt5_Prefix "${Qt5_DIR}/../../../.." ABSOLUTE)
endif()
if (USE_QQUICKWIDGET)
    find_package(Qt5 5.9 REQUIRED QuickWidgets)
elseif(NOT DISABLE_QQUICKWIDGET)
    # QQuickWidget only stopped crashing in Qt 5.12, use it by default if found
    find_package(Qt5 5.12 QUIET COMPONENTS QuickWidgets)
    if (Qt5QuickWidgets_FOUND)
        set(USE_QQUICKWIDGET ON)
    endif()
endif()

if(WIN32)
    enable_language(RC)
    include(CMakeDetermineRCCompiler)

    if(MINGW)
        set(CMAKE_RC_COMPILER_INIT windres)
        set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> <FLAGS> -O coff <DEFINES> -I${CMAKE_CURRENT_BINARY_DIR} -i <SOURCE> -o <OBJECT>")
    endif()
endif()

if ((NOT DEFINED USE_INTREE_LIBQMC OR USE_INTREE_LIBQMC)
        AND EXISTS ${PROJECT_SOURCE_DIR}/lib/lib/util.h)
    add_subdirectory(lib EXCLUDE_FROM_ALL)
    include_directories(lib)
    if (NOT DEFINED USE_INTREE_LIBQMC)
        set (USE_INTREE_LIBQMC 1)
    endif ()
endif ()
if (NOT USE_INTREE_LIBQMC)
    find_package(Quotient 0.6 REQUIRED)
    if (NOT Quotient_FOUND)
        message( WARNING "libQuotient not found; configuration will most likely fail.")
        message( WARNING "Make sure you have installed libQuotient development files")
        message( WARNING "as a package or checked out the library sources in lib/.")
        message( WARNING "See also BUILDING.md")
    endif ()
endif ()

find_package(Qt5Keychain QUIET)
if (Qt5Keychain_FOUND)
    set(USE_KEYCHAIN ON)
endif()

message( STATUS )
message( STATUS "=============================================================================" )
message( STATUS "                          Quaternion Build Information" )
message( STATUS "=============================================================================" )
if (CMAKE_BUILD_TYPE)
    message( STATUS "Build type: ${CMAKE_BUILD_TYPE}")
endif(CMAKE_BUILD_TYPE)
message( STATUS "Quaternion install prefix: ${CMAKE_INSTALL_PREFIX}" )
# Get Git info if possible
find_package(Git)
if(GIT_FOUND)
    execute_process(COMMAND
        "${GIT_EXECUTABLE}" rev-parse -q HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_SHA1
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    message( STATUS "Git SHA1: ${GIT_SHA1}")
endif()
message( STATUS "Using compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}" )
message( STATUS "Using Qt ${Qt5_VERSION} at ${Qt5_Prefix}" )
if (USE_INTREE_LIBQMC)
    message( STATUS "Using in-tree libQuotient")
    if (GIT_FOUND)
        execute_process(COMMAND
            "${GIT_EXECUTABLE}" rev-parse -q HEAD
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib
            OUTPUT_VARIABLE LIB_GIT_SHA1
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        message( STATUS "  Library git SHA1: ${LIB_GIT_SHA1}")
    endif (GIT_FOUND)
else ()
    message( STATUS "Using libQuotient ${Quotient_VERSION} at ${Quotient_DIR}")
endif ()
if (USE_QQUICKWIDGET)
    message( STATUS "Using QQuickWidget to render QML")
endif(USE_QQUICKWIDGET)
if (USE_KEYCHAIN)
    message( STATUS "Using Qt Keychain ${Qt5Keychain_VERSION} at ${Qt5Keychain_DIR}")
endif ()
message( STATUS "=============================================================================" )
message( STATUS )

# Set up source files
set(quaternion_SRCS
    client/quaternionroom.cpp
    client/imageprovider.cpp
    client/activitydetector.cpp
    client/dialog.cpp
    client/logindialog.cpp
    client/networkconfigdialog.cpp
    client/roomdialogs.cpp
    client/mainwindow.cpp
    client/roomlistdock.cpp
    client/userlistdock.cpp
    client/kchatedit.cpp
    client/chatedit.cpp
    client/chatroomwidget.cpp
    client/systemtrayicon.cpp
    client/models/messageeventmodel.cpp
    client/models/userlistmodel.cpp
    client/models/roomlistmodel.cpp
    client/models/abstractroomordering.cpp
    client/models/orderbytag.cpp
    client/main.cpp
    )

set(quaternion_QRC
    client/resources.qrc
    )

# quaternion_en.ts is updated explicitly by building trbase target,
# while all other translation files are created and updated externally at
# Lokalise.co
set(quaternion_en_TS client/translations/quaternion_en.ts)
QT5_CREATE_TRANSLATION(client/translations ${quaternion_SRCS} ${quaternion_QRC} ${quaternion_en_TS})
add_custom_target(trbase SOURCES ${quaternion_en_TS} VERBATIM)

set(quaternion_TS
    client/translations/quaternion_en_GB.ts
    client/translations/quaternion_de_DE.ts
    client/translations/quaternion_pl.ts
    client/translations/quaternion_ru.ts
    )
QT5_ADD_TRANSLATION(quaternion_QM ${quaternion_TS})

QT5_ADD_RESOURCES(quaternion_QRC_SRC ${quaternion_QRC})
set_property(SOURCE qrc_resources.cpp PROPERTY SKIP_AUTOMOC ON)

if(WIN32)
    set(quaternion_WINRC quaternion_win32.rc)
    set_property(SOURCE quaternion_win32.rc APPEND PROPERTY
        OBJECT_DEPENDS ${PROJECT_SOURCE_DIR}/icons/quaternion.ico
    )
endif()

if(APPLE)
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
endif(APPLE)

# Windows, this is a GUI executable; OSX, make a bundle
add_executable(${PROJECT_NAME} WIN32 MACOSX_BUNDLE
    ${quaternion_SRCS} ${quaternion_QRC_SRC} ${quaternion_QM}
    ${quaternion_WINRC} ${${PROJECT_NAME}_MAC_ICON})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    GIT_SHA1="${GIT_SHA1}" LIB_GIT_SHA1="${LIB_GIT_SHA1}")
target_link_libraries(${PROJECT_NAME}
    Quotient Qt5::Widgets Qt5::Quick Qt5::Qml Qt5::Gui Qt5::Network)

if (USE_QQUICKWIDGET)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_QQUICKWIDGET)
    target_link_libraries(${PROJECT_NAME} Qt5::QuickWidgets)
endif()

if(USE_KEYCHAIN)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_KEYCHAIN)
    target_link_libraries(${PROJECT_NAME} ${QTKEYCHAIN_LIBRARIES})
    include_directories(${QTKEYCHAIN_INCLUDE_DIR})
endif()

# macOS specific config for bundling
if (APPLE)
    set_property(TARGET ${PROJECT_NAME} PROPERTY MACOSX_BUNDLE_INFO_PLIST
                 "${PROJECT_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in")
endif()

# Installation

if (NOT CMAKE_INSTALL_BINDIR)
    set(CMAKE_INSTALL_BINDIR ".")
endif()

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR})
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
    ecm_install_icons(ICONS ${quaternion_icons} icons/quaternion/sc-apps-quaternion.svgz
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
            COMMAND \"${Qt5_BinDir}/windeployqt\" --verbose ${DEPLOY_VERBOSITY}
                --no-multimediaquick --no-declarative --no-test --no-winextras
                --qmldir \"${QML_DIR}\"
                \${CMAKE_INSTALL_PREFIX}\${CMAKE_INSTALL_BINDIR}
            RESULT_VARIABLE WDQ_RETVAL
        )
        if (WDQ_RETVAL)
            message( \"windeployqt returned \${WDQ_RETVAL} - check messages above\")
        else()
            message( STATUS \"Quaternion and its dependencies have been deployed to \${CMAKE_INSTALL_PREFIX}.\")
        endif()
    ")
    install(FILES ${quaternion_QM}
            DESTINATION ${CMAKE_INSTALL_BINDIR}/translations
           )
endif(WIN32)

# Packaging

if(APPLE)
    set(MACDEPLOYQT_ARGS ${PROJECT_NAME}.app -dmg -qmldir="${QML_DIR}" -verbose=${DEPLOY_VERBOSITY})
    add_custom_target(image
        COMMAND "${Qt5_BinDir}/macdeployqt" ${MACDEPLOYQT_ARGS}
        DEPENDS ${PROJECT_NAME}
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
        COMMENT "Running ${MACDEPLOYQT} with args: ${MACDEPLOYQT_ARGS}"
    )
endif(APPLE)
