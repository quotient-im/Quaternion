import QtQuick 2.0
import QtQuick.Layouts 1.1

Attachment {
    required property var sourceSize
    required property url source
    required property var maxHeight
    required property bool autoload
    openOnFinished: false

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

        TimelineMouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            hoverEnabled: true

            onContainsMouseChanged:
                controller.showStatusMessage(containsMouse
                                             ? room.fileSource(eventId) : "")
            onClicked: {
                openOnFinished = true
                openExternally()
            }
        }

        TimelineMouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            cursorShape: Qt.PointingHandCursor
            onClicked: controller.showMenu(index, textFieldImpl.hoveredLink,
                textFieldImpl.selectedText, showingDetails)
        }

        Component.onCompleted:
            if (visible && autoload && !downloaded && !(progressInfo && progressInfo.isUpload))
                room.downloadFile(eventId)
    }

}
