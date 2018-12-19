CMAKE_MINIMUM_REQUIRED(VERSION 3.1)

project(quaternion CXX)

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

set(CMAKE_CXX_STANDARD 14)

# Setup command line parameters for the compiler and linker
foreach (FLAG "" all pedantic extra no-unused-parameter)
    CHECK_CXX_COMPILER_FLAG("-W${FLAG}" WARN_${FLAG}_SUPPORTED)
    if ( WARN_${FLAG}_SUPPORTED AND NOT CMAKE_CXX_FLAGS MATCHES "(^| )-W?${FLAG}($| )")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W${FLAG}")
    endif ()
endforeach ()

# Find the libraries
find_package(Qt5 5.7 REQUIRED Widgets Network Quick Qml QuickWidgets Gui LinguistTools)
if (USE_QQUICKWIDGET)
    find_package(Qt5 5.7 REQUIRED QuickWidgets)
endif()
get_filename_component(Qt5_Prefix "${Qt5_DIR}/../../../.." ABSOLUTE)

if(WIN32)
    enable_language(RC)
    include(CMakeDetermineRCCompiler)

    if(MINGW)
        set(CMAKE_RC_COMPILER_INIT windres)
        set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> <FLAGS> -O coff <DEFINES> -I${CMAKE_CURRENT_BINARY_DIR} -i <SOURCE> -o <OBJECT>")
    endif()
endif()

if (USE_INTREE_LIBQMC)
    add_subdirectory(lib EXCLUDE_FROM_ALL)
    include_directories(lib)
else ()
    find_package(QMatrixClient 0.4 REQUIRED)
    if (NOT QMatrixClient_FOUND)
        message( WARNING "libQMatrixClient not found; configuration will most likely fail.")
        message( WARNING "Make sure you have installed libQMatrixClient development files")
        message( WARNING "or pass -DUSE_INTREE_LIBQMC to CMake if you're using the library")
        message( WARNING "as a Git submodule in lib/. See also BUILDING.md")
    endif ()
endif ()

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
    message( STATUS "Using in-tree libQMatrixClient")
    if (GIT_FOUND)
        execute_process(COMMAND
            "${GIT_EXECUTABLE}" rev-parse -q HEAD
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib
            OUTPUT_VARIABLE LIB_GIT_SHA1
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        message( STATUS "  Library git SHA1: ${LIB_GIT_SHA1}")
    endif (GIT_FOUND)
else ()
    message( STATUS "Using libQMatrixClient ${QMatrixClient_VERSION} at ${QMatrixClient_DIR}")
endif ()
if (USE_QQUICKWIDGET)
    message( STATUS "Using QQuickWidget to render QML")
endif(USE_QQUICKWIDGET)
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
    client/main.cpp
    )

set(quaternion_QRC
    client/resources.qrc
    )

set(quaternion_TS
    client/translations/quaternion_en.ts
    client/translations/quaternion_de_DE.ts
    client/translations/quaternion_pl.ts
    )

QT5_ADD_RESOURCES(quaternion_QRC_SRC ${quaternion_QRC})
set_property(SOURCE qrc_resources.cpp PROPERTY SKIP_AUTOMOC ON)

QT5_ADD_TRANSLATION(quaternion_QM ${quaternion_TS})
QT5_CREATE_TRANSLATION(client/translations ${quaternion_SRCS} ${quaternion_QRC} ${quaternion_TS})

if(WIN32)
    set(quaternion_WINRC ${CMAKE_CURRENT_SOURCE_DIR}/quaternion_win32.rc)
    set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/quaternion_win32.rc
        APPEND PROPERTY OBJECT_DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/icons/quaternion.ico
    )
endif()

if(APPLE)
    set(ICON_NAME "quaternion.icns")
    set(ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/icons/${ICON_NAME}")

    set(MACOSX_BUNDLE_ICON_FILE ${ICON_NAME})

    set_source_files_properties("${ICON_PATH}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif(APPLE)

# Windows, this is a GUI executable; OSX, make a bundle
add_executable(quaternion WIN32 MACOSX_BUNDLE ${quaternion_SRCS} ${quaternion_QRC_SRC} ${quaternion_WINRC} ${quaternion_QM} ${ICON_PATH})

target_link_libraries(quaternion QMatrixClient Qt5::Widgets Qt5::Quick Qt5::Qml Qt5::Gui Qt5::Network)
target_compile_definitions(quaternion PRIVATE GIT_SHA1="${GIT_SHA1}" LIB_GIT_SHA1="${LIB_GIT_SHA1}")

if (USE_QQUICKWIDGET)
    target_compile_definitions(quaternion PRIVATE USE_QQUICKWIDGET)
    target_link_libraries(quaternion Qt5::QuickWidgets)
endif()

# macOS specific config for bundling
set_target_properties(quaternion PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in")

# Installation

if (NOT CMAKE_INSTALL_BINDIR)
    set(CMAKE_INSTALL_BINDIR ".")
endif()

install(TARGETS quaternion
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR})
if(LINUX)
    install(FILES linux/quaternion.desktop
            DESTINATION  ${CMAKE_INSTALL_DATADIR}/applications
            )
    install(FILES linux/com.github.quaternion.appdata.xml
            DESTINATION  ${CMAKE_INSTALL_DATADIR}/metainfo
            )
    install(FILES ${quaternion_QM}
            DESTINATION ${CMAKE_INSTALL_DATADIR}/QMatrixClient/quaternion/translations
            )
    file(GLOB quaternion_icons icons/quaternion/*-apps-quaternion.png)
    ecm_install_icons(ICONS ${quaternion_icons} icons/quaternion/sc-apps-quaternion.svgz
                      DESTINATION ${CMAKE_INSTALL_DATADIR}/icons
                      )
endif(LINUX)

if(WIN32)
    if (${Qt5_VERSION} VERSION_LESS "5.3")
        install(CODE "
            message(\"Deploying on Windows is only supported with Qt 5.3 or higher\")
            message(\"Bare executable has been copied to the target folder\")
        ")
    else()
        get_filename_component(Qt5_WinBaseDir "${Qt5_DIR}/../../.." ABSOLUTE)
        install(CODE "
            if (CMAKE_INSTALL_CONFIG_NAME STREQUAL \"Debug\")
                set(WDQ_FLAG debug)
            else()
                set(WDQ_FLAG release)
            endif()
            execute_process(
                COMMAND bin/windeployqt --\${WDQ_FLAG} --no-system-d3d-compiler --no-opengl
                    --no-multimedia --no-multimediaquick --no-declarative --no-test \
                    --qmldir qml \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\"
                WORKING_DIRECTORY \"${Qt5_WinBaseDir}\"
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
    endif()
endif(WIN32)
