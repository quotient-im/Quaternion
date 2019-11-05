import QtQuick 2.6
import QtQuick.Controls 2.2

ToolButton {
    id: timelineItemToolButton
    width: visible * implicitWidth
    height: visible * parent.height
    anchors.top: parent.top
    anchors.rightMargin: 2

    contentItem: Text {
        text: timelineItemToolButton.text
        font: settings.font
        fontSizeMode: Text.VerticalFit
        minimumPointSize: settings.font.pointSize - 3
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        renderType: settings.render_type
    }
}
