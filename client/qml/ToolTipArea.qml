import QtQuick 2.0

MouseArea {
    property alias tooltip: tooltip
    property alias text: tooltip.text

    anchors.fill: parent
    acceptedButtons: Qt.NoButton
    hoverEnabled: true

    ToolTip { id: tooltip; visible: containsMouse }
}
