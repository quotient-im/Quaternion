import QtQuick 2.0
import QtQuick.Controls 2.1
import Quotient 1.0

ToolTip {
    id:tooltip
    TimelineSettings { id: settings }

    delay: settings.animations_duration_ms
    verticalPadding: 2
    font: settings.font

    background: Rectangle {
        SystemPalette { id: palette; colorGroup: SystemPalette.Active }
        radius: 3
        color: palette.window
        border.width: 1
        border.color: palette.windowText
    }
    enter: AnimatedTransition { NormalNumberAnimation {
        target: tooltip
        property: "opacity"
        easing.type: Easing.OutQuad
        from: 0
        to: 0.9
    } }
    exit: AnimatedTransition { NormalNumberAnimation {
        target: tooltip
        property: "opacity"
        easing.type: Easing.InQuad
        to: 0
    } }
}
