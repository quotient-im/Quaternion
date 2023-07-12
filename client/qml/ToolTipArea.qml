import QtQuick 2.0
import QtQuick.Controls 2.3

MouseArea {
    required property string text

    anchors.fill: parent
    acceptedButtons: Qt.NoButton
    hoverEnabled: true

    ToolTip { id: tooltip; text: parent.text; visible: containsMouse }
}
