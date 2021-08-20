import QtQuick 2.0
import Quotient 1.0

NumberAnimation {
    property var settings: TimelineSettings { }
    duration: settings.animations_duration_ms
}
