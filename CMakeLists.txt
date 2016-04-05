cmake_minimum_required(VERSION 3.1) # Because of target_compile_features

project(quaternion)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

# Find the libraries
find_package(Qt5Widgets 5.3.0)
find_package(Qt5Network 5.3.0)
find_package(Qt5Quick 5.3.0)
find_package(Qt5Qml 5.3.0)
find_package(Qt5Gui 5.3.0)

message( STATUS )
message( STATUS "================================================================================" )
message( STATUS "                          Quaternion Build Information                          " )
message( STATUS "================================================================================" )
message( STATUS "Building with: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}" )
message( STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}" )
message( STATUS "Path to Qt Core: ${Qt5Core_DIR}" )
message( STATUS "================================================================================" )
message( STATUS )

add_subdirectory(lib)

# Set up source files
set(quaternion_SRCS
    client/quaternionconnection.cpp
    client/quaternionroom.cpp
    client/logindialog.cpp
    client/mainwindow.cpp
    client/roomlistdock.cpp
    client/userlistdock.cpp
    client/chatroomwidget.cpp
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

# Setup command line parameters for the compiler and linker
if ( CMAKE_COMPILER_IS_GNUCC )
    add_compile_options( "-Wall" )
endif ( CMAKE_COMPILER_IS_GNUCC )
target_compile_features(quaternion PRIVATE cxx_range_for)
target_compile_features(quaternion PRIVATE cxx_override)

target_link_libraries(quaternion qmatrixclient Qt5::Widgets Qt5::Quick Qt5::Qml Qt5::Gui Qt5::Network)
if ( KF5CoreAddons_FOUND )
    # The proper way of doing things would be to make a separate config.h.in
    # file and use configure_file() command here to generate config.h with
    # needed C++ preprocessor macros. If we have more than one or two
    # dependencies like that, we should turn to that more scalable way.
    # As for now, passing a macro through -D is easier to observe and maintain.
    target_compile_definitions ( quaternion PRIVATE USING_SYSTEM_KCOREADDONS )
    target_link_libraries(quaternion KF5::CoreAddons)
else ( KF5CoreAddons_FOUND )
    include_directories( lib lib/kcoreaddons/src/lib/jobs )
endif ( KF5CoreAddons_FOUND )
