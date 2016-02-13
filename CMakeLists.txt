cmake_minimum_required(VERSION 2.8.11)

project(quaternion)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

# Find the QtWidgets library
find_package(Qt5Widgets 5.4.0)
find_package(Qt5Network 5.4.0)
find_package(Qt5Quick 5.4.0)
find_package(Qt5Qml 5.4.0)
<<<<<<< HEAD
#find_package(KF5CoreAddons)

include_directories( kcoreaddons/src/lib )
=======
find_package(Qt5Gui 5.4.0)
find_package(KF5CoreAddons)
>>>>>>> master

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
    kcoreaddons/src/lib/jobs/kjob.cpp
    kcoreaddons/src/lib/jobs/kcompositejob.cpp
    kcoreaddons/src/lib/jobs/kjobtrackerinterface.cpp
    kcoreaddons/src/lib/jobs/kjobuidelegate.cpp
    )

set(qmatrixclient_QRC
    client/resources.qrc
    )

QT5_ADD_RESOURCES(qmatrixclient_QRC_SRC ${qmatrixclient_QRC})

# Tell CMake to create the executable
add_executable(quaternion ${qmatrixclient_SRCS} ${qmatrixclient_QRC_SRC})

if ( CMAKE_COMPILER_IS_GNUCC )
    # This is tested
    set_property( TARGET quaternion APPEND_STRING PROPERTY COMPILE_FLAGS -Wall )
endif ( CMAKE_COMPILER_IS_GNUCC )
target_compile_features(quaternion PRIVATE cxx_range_for)
target_compile_features(quaternion PRIVATE cxx_override)

# Use the Widgets module from Qt 5.
target_link_libraries(quaternion Qt5::Widgets Qt5::Quick Qt5::Qml Qt5::Gui Qt5::Network) #KF5::CoreAddons)
