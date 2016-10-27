CMAKE_MINIMUM_REQUIRED(VERSION 2.8.11)

project(quaternion CXX)

include(CheckCXXCompilerFlag)
include(GNUInstallDirs)
include(cmake/ECMInstallIcons.cmake)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

if(UNIX AND NOT APPLE)
    set(LINUX 1)
endif(UNIX AND NOT APPLE)

# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Debug' as none was specified")
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build" FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
endif()

# Setup command line parameters for the compiler and linker
CHECK_CXX_COMPILER_FLAG("-Wall" WALL_FLAG_SUPPORTED)
if ( WALL_FLAG_SUPPORTED )
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif ( WALL_FLAG_SUPPORTED )

if ( CMAKE_VERSION VERSION_LESS "3.1" )
    CHECK_CXX_COMPILER_FLAG("-std=c++11" STD_FLAG_SUPPORTED)
    if ( STD_FLAG_SUPPORTED )
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    endif ( STD_FLAG_SUPPORTED )
else ( CMAKE_VERSION VERSION_LESS "3.1" )
    set(CMAKE_CXX_STANDARD 11)
endif ( CMAKE_VERSION VERSION_LESS "3.1" )

# Find the libraries
find_package(Qt5 5.2.1 REQUIRED Widgets Network Quick Qml Gui)
get_filename_component(Qt5_Prefix "${Qt5_DIR}/../../../.." ABSOLUTE)

message( STATUS )
message( STATUS "=============================================================================" )
message( STATUS "                          Quaternion Build Information" )
message( STATUS "=============================================================================" )
if (CMAKE_BUILD_TYPE)
    message( STATUS "Build type: ${CMAKE_BUILD_TYPE}")
endif(CMAKE_BUILD_TYPE)
message( STATUS "Using compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}" )
message( STATUS "Using Qt ${Qt5_VERSION} at: ${Qt5_Prefix}" )
message( STATUS "Quaternion install prefix: ${CMAKE_INSTALL_PREFIX}" )
message( STATUS "=============================================================================" )
message( STATUS )

add_subdirectory(lib)
include_directories(lib)

# Set up source files
set(quaternion_SRCS
    client/quaternionconnection.cpp
    client/quaternionroom.cpp
    client/message.cpp
    client/imageprovider.cpp
    client/logindialog.cpp
    client/mainwindow.cpp
    client/roomlistdock.cpp
    client/userlistdock.cpp
    client/chatroomwidget.cpp
    client/systemtray.cpp
    client/models/messageeventmodel.cpp
    client/models/userlistmodel.cpp
    client/models/roomlistmodel.cpp
    client/main.cpp
    )

set(quaternion_QRC
    client/resources.qrc
    )

QT5_ADD_RESOURCES(quaternion_QRC_SRC ${quaternion_QRC})

# Tell CMake to create the executable
# (and that on Windows it should be a GUI executable)
# TODO: MacOS builders, should MACOSX_BUNDLE be specified here as well?
add_executable(quaternion WIN32 ${quaternion_SRCS} ${quaternion_QRC_SRC})

target_link_libraries(quaternion qmatrixclient Qt5::Widgets Qt5::Quick Qt5::Qml Qt5::Gui Qt5::Network)

# Installation

install(TARGETS quaternion
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(LINUX)
    install(FILES linux/quaternion.desktop
            DESTINATION  ${CMAKE_INSTALL_DATADIR}/applications
            )
    file(GLOB quaternion_icons icons/quaternion/*-apps-quaternion.png)
    ecm_install_icons(ICONS ${quaternion_icons} icons/quaternion/sc-apps-quaternion.svgz
                      DESTINATION ${CMAKE_INSTALL_DATADIR}/icons
                      )
endif(LINUX)

if(WIN32)
    install(CODE "
        if (CMAKE_INSTALL_CONFIG_NAME STREQUAL \"Debug\")
            set(WDQ_FLAG debug)
        else()
            set(WDQ_FLAG release)
        endif()
        execute_process(
            COMMAND bin/windeployqt -\${WDQ_FLAG} --no-system-d3d-compiler --no-opengl
                --no-multimedia --no-multimediaquick --no-declarative --no-test \
                --qmldir qml \"\${CMAKE_INSTALL_PREFIX}/\${CMAKE_INSTALL_BINDIR}\"
            WORKING_DIRECTORY \"\${Qt5_Prefix}/..\"
        )
    ")
endif()
