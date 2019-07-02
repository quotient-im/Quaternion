import QtQuick 2.0
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.1

Attachment {
    property var sourceSize
    property url source
    property var maxHeight
    property bool autoload

    Image {
        id: imageContent
        width: parent.width
        height: sourceSize.height *
                Math.min(maxHeight / sourceSize.height * 0.9,
                         Math.min(width / sourceSize.width, 1))
        fillMode: Image.PreserveAspectFit
        horizontalAlignment: Image.AlignLeft

        source: parent.source
        sourceSize: parent.sourceSize

//        Behavior on height { NumberAnimation {
//            duration: settings.fast_animations_duration_ms
//            easing.type: Easing.OutQuad
//        }}

        TimelineMouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            hoverEnabled: true

            onContainsMouseChanged:
                controller.showStatusMessage(containsMouse
                                             ? room.fileSource(eventId) : "")
            onClicked: openExternally()
        }

        TimelineMouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            cursorShape: Qt.PointingHandCursor
            onClicked: controller.showMenu(index, textFieldImpl.hoveredLink,
                showingDetails)
        }

        Component.onCompleted:
            if (visible && autoload && !downloaded && !(progressInfo && progressInfo.isUpload))
                room.downloadFile(eventId)
    }

}
