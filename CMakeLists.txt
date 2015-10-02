cmake_minimum_required(VERSION 2.8.11)

project(qmatrixclient)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

# Find the QtWidgets library
find_package(Qt5Widgets 5.4.0)
find_package(Qt5Network 5.4.0)
find_package(KF5CoreAddons)

set(qmatrixclient_SRCS
    lib/connectiondata.cpp
    lib/connection.cpp
    lib/connectionprivate.cpp
    lib/room.cpp
    lib/logmessage.cpp
    lib/state.cpp
    lib/events/event.cpp
    lib/events/roommessageevent.cpp
    lib/events/roomaliasesevent.cpp
    lib/jobs/basejob.cpp
    lib/jobs/checkauthmethods.cpp
    lib/jobs/passwordlogin.cpp
    lib/jobs/initialsyncjob.cpp
    lib/jobs/geteventsjob.cpp
    lib/jobs/postmessagejob.cpp
    lib/jobs/joinroomjob.cpp
    client/logindialog.cpp
    client/mainwindow.cpp
    client/roomlistdock.cpp
    client/chatroomwidget.cpp
    client/models/messageeventmodel.cpp
    client/main.cpp
    )

# Tell CMake to create the executable
add_executable(qmatrixclient ${qmatrixclient_SRCS})

if ( CMAKE_COMPILER_IS_GNUCC )
    # This is tested
    set_property( TARGET qmatrixclient APPEND_STRING PROPERTY COMPILE_FLAGS -Wall )
endif ( CMAKE_COMPILER_IS_GNUCC )
target_compile_features(qmatrixclient PRIVATE cxx_range_for)
target_compile_features(qmatrixclient PRIVATE cxx_override)

# Use the Widgets module from Qt 5.
target_link_libraries(qmatrixclient Qt5::Widgets Qt5::Network KF5::CoreAddons)
