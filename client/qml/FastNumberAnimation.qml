import QtQuick 2.0
import Quotient 1.0

NumberAnimation {
    property var settings: TimelineSettings { }
    duration: settings.fast_animations_duration_ms
}
