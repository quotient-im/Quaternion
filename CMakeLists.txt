cmake_minimum_required(VERSION 2.8.11)

project(qmatrixclient)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

# Find the QtWidgets library
find_package(Qt5Widgets)
find_package(Qt5Network)
find_package(KF5CoreAddons)

set(qmatrixclient_SRCS
    lib/connection.cpp
    lib/room.cpp
    lib/logmessage.cpp
    lib/jobs/basejob.cpp
    lib/jobs/checkauthmethods.cpp
    lib/jobs/passwordlogin.cpp
    lib/jobs/initialsyncjob.cpp
    client/logindialog.cpp
    client/mainwindow.cpp
    client/roomlistdock.cpp
    client/main.cpp
    )

# Tell CMake to create the executable
add_executable(qmatrixclient ${qmatrixclient_SRCS})

if ( CMAKE_COMPILER_IS_GNUCC )
    # This is tested
    set_property( TARGET qmatrixclient APPEND_STRING PROPERTY COMPILE_FLAGS -Wall )
endif ( CMAKE_COMPILER_IS_GNUCC )
target_compile_features(qmatrixclient PRIVATE cxx_range_for)

# Use the Widgets module from Qt 5.
target_link_libraries(qmatrixclient Qt5::Widgets Qt5::Network KF5::CoreAddons)
