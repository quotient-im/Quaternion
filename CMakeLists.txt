CMAKE_MINIMUM_REQUIRED(VERSION 2.8.11) # Maybe works with an even older CMake

project(quaternion)
enable_language(CXX)

include(CheckCXXCompilerFlag)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

# Find the libraries
find_package(Qt5Widgets 5.2.1)
find_package(Qt5Network 5.2.1)
find_package(Qt5Quick 5.2.1)
find_package(Qt5Qml 5.2.1)
find_package(Qt5Gui 5.2.1)

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
    target_compile_features(quaternion PRIVATE cxx_range_for)
    target_compile_features(quaternion PRIVATE cxx_override)
    target_compile_features(quaternion PRIVATE cxx_auto_type)
    target_compile_features(quaternion PRIVATE cxx_nullptr)
endif ( CMAKE_VERSION VERSION_LESS "3.1" )

target_link_libraries(quaternion qmatrixclient Qt5::Widgets Qt5::Quick Qt5::Qml Qt5::Gui Qt5::Network)
