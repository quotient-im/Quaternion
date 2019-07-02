import QtQuick 2.2
import QtQuick.Controls 1.4

Label {
    signal clicked

    font.italic: true
    textFormat: Text.PlainText
    TimelineMouseArea
    {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        acceptedButtons: Qt.LeftButton
        onClicked: parent.clicked()
    }
}
