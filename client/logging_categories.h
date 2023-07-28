#pragma once

// NB: Only include this file from .cpp

#include <QtCore/QLoggingCategory>

// Reusing the macro defined in Quotient - these must never cross ways
#define QUO_LOGGING_CATEGORY(Name, Id) \
    inline Q_LOGGING_CATEGORY((Name), (Id), QtInfoMsg)

namespace {
QUO_LOGGING_CATEGORY(MAIN, "quaternion.main")
QUO_LOGGING_CATEGORY(ACCOUNTSELECTOR, "quaternion.accountselector")
QUO_LOGGING_CATEGORY(MODELS, "quaternion.models")
QUO_LOGGING_CATEGORY(EVENTMODEL, "quaternion.models.events")
QUO_LOGGING_CATEGORY(TIMELINE, "quaternion.timeline")
QUO_LOGGING_CATEGORY(HTMLFILTER, "quaternion.htmlfilter")
QUO_LOGGING_CATEGORY(MSGINPUT, "quaternion.messageinput")
QUO_LOGGING_CATEGORY(IMAGEPROVIDER, "quaternion.imageprovider")

// Only to be used in QML; shows up here for documentation purpose only
[[maybe_unused]] QUO_LOGGING_CATEGORY(TIMELINEQML, "quaternion.timeline.qml")
}
