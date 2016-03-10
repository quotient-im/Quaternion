cmake_minimum_required(VERSION 3.1) # Because of target_compile_features

project(quaternion)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

# Whether to build with the bundled KCoreAddons or system KCoreAddons
set( BUNDLE_KCOREADDONS "AUTO" CACHE STRING "Build own KCoreAddons, one of ON, OFF and AUTO" )

# Find the libraries
find_package(Qt5Widgets 5.3.0)
find_package(Qt5Network 5.3.0)
find_package(Qt5Quick 5.3.0)
find_package(Qt5Qml 5.3.0)
find_package(Qt5Gui 5.3.0)

if ( (NOT BUNDLE_KCOREADDONS STREQUAL "ON")
     AND (NOT BUNDLE_KCOREADDONS STREQUAL "OFF")
     AND (NOT BUNDLE_KCOREADDONS STREQUAL "AUTO") )
       message( FATAL_ERROR "BUNDLE_KCOREADDONS must be one of ON, OFF or AUTO" )
endif ()

if ( BUNDLE_KCOREADDONS STREQUAL "AUTO" )
    find_package(KF5CoreAddons QUIET)
elseif ( BUNDLE_KCOREADDONS STREQUAL "OFF" )
    find_package(KF5CoreAddons REQUIRED)
endif ()

message( STATUS )
message( STATUS "================================================================================" )
message( STATUS "                          Quaternion Build Information                          " )
message( STATUS "================================================================================" )
message( STATUS "Building with: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}" )
message( STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}" )
message( STATUS "Path to Qt Core: ${Qt5Core_DIR}" )
message( STATUS "Build own KCoreAddons (BUNDLE_KCOREADDONS): ${BUNDLE_KCOREADDONS}" )
if ( NOT BUNDLE_KCOREADDONS STREQUAL "ON" )
    if ( KF5CoreAddons_FOUND )
        message( STATUS "'- Path to system KCoreAddons: ${KF5CoreAddons_DIR}" )
    else ( KF5CoreAddons_FOUND )
        message( STATUS "'- System KCoreAddons not found, using the bundled version" )
    endif ( KF5CoreAddons_FOUND )
endif ( NOT BUNDLE_KCOREADDONS STREQUAL "ON" )
message( STATUS "================================================================================" )
message( STATUS )

# Set up source files
set(qmatrixclient_SRCS
    lib/connectiondata.cpp
    lib/connection.cpp
    lib/connectionprivate.cpp
    lib/room.cpp
    lib/user.cpp
    lib/logmessage.cpp
    lib/state.cpp
    lib/events/event.cpp
    lib/events/roommessageevent.cpp
    lib/events/roomnameevent.cpp
    lib/events/roomaliasesevent.cpp
    lib/events/roomcanonicalaliasevent.cpp
    lib/events/roommemberevent.cpp
    lib/events/roomtopicevent.cpp
    lib/events/typingevent.cpp
    lib/events/unknownevent.cpp
    lib/jobs/basejob.cpp
    lib/jobs/checkauthmethods.cpp
    lib/jobs/passwordlogin.cpp
    lib/jobs/postmessagejob.cpp
    lib/jobs/joinroomjob.cpp
    lib/jobs/leaveroomjob.cpp
    lib/jobs/roommembersjob.cpp
    lib/jobs/roommessagesjob.cpp
    lib/jobs/syncjob.cpp
    lib/jobs/mediathumbnailjob.cpp
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

# Add bundled KCoreAddons sources if we haven't found the system sources
# or if we ignore them
if ( NOT KF5CoreAddons_FOUND )
    set (qmatrixclient_SRCS ${qmatrixclient_SRCS}
        kcoreaddons/src/lib/jobs/kjob.cpp
        kcoreaddons/src/lib/jobs/kcompositejob.cpp
        kcoreaddons/src/lib/jobs/kjobtrackerinterface.cpp
        kcoreaddons/src/lib/jobs/kjobuidelegate.cpp
        )
endif ( NOT KF5CoreAddons_FOUND )

set(qmatrixclient_QRC
    client/resources.qrc
    )

QT5_ADD_RESOURCES(qmatrixclient_QRC_SRC ${qmatrixclient_QRC})

# Tell CMake to create the executable
add_executable(quaternion ${qmatrixclient_SRCS} ${qmatrixclient_QRC_SRC})

# Setup command line parameters for the compiler and linker
if ( CMAKE_COMPILER_IS_GNUCC )
    add_compile_options( "-Wall" )
endif ( CMAKE_COMPILER_IS_GNUCC )
target_compile_features(quaternion PRIVATE cxx_range_for)
target_compile_features(quaternion PRIVATE cxx_override)

target_link_libraries(quaternion Qt5::Widgets Qt5::Quick Qt5::Qml Qt5::Gui Qt5::Network)
if ( KF5CoreAddons_FOUND )
    # The proper way of doing things would be to make a separate config.h.in
    # file and use configure_file() command here to generate config.h with
    # needed C++ preprocessor macros. If we have more than one or two
    # dependencies like that, we should turn to that more scalable way.
    # As for now, passing a macro through -D is easier to observe and maintain.
    target_compile_definitions ( quaternion PRIVATE USING_SYSTEM_KCOREADDONS )
    target_link_libraries(quaternion KF5::CoreAddons)
else ( KF5CoreAddons_FOUND )
    include_directories( kcoreaddons/src/lib/jobs )
endif ( KF5CoreAddons_FOUND )
