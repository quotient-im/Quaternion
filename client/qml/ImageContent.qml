import QtQuick 2.15
import QtQuick.Controls 2.15
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

        HoverHandler {
            id: imageHoverHandler
            cursorShape: Qt.PointingHandCursor
        }
        ToolTip.visible: imageHoverHandler.hovered
        ToolTip.text: room.fileSource(eventId)

        TapHandler {
            acceptedButtons: Qt.LeftButton
            onTapped: {
                openOnFinished = true
                openExternally()
            }
        }
        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: controller.showMenu(index, textFieldImpl.hoveredLink,
                                          textFieldImpl.selectedText,
                                          showingDetails)
        }

        Component.onCompleted:
            if (visible && autoload && !downloaded && !(progressInfo && progressInfo.isUpload))
                room.downloadFile(eventId)
    }

}
