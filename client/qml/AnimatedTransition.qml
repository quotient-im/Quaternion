import QtQuick 2.0
import Quotient 1.0

Transition {
    property var settings: TimelineSettings { }
    enabled: settings.enable_animations
}
