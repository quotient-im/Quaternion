import QtQuick 2.15
import QtQuick.Controls 2.15

Attachment {
    id: content

    required property var sourceSize
    required property url source
    required property var maxHeight
    required property bool autoload
    openOnFinished: false

    Image {
        width: parent.width
        height: sourceSize.height * Math.min(parent.maxHeight / sourceSize.height * 0.9,
                                             Math.min(width / sourceSize.width, 1))
        fillMode: Image.PreserveAspectFit
        horizontalAlignment: Image.AlignLeft

        // The spec says that the attachment URL SHOULD be mxc but is not required to be
        source: parent.source.toString().startsWith("mxc")
                ? room.makeMediaUrl(eventId, parent.source) : parent.source
        sourceSize: parent.sourceSize

        HoverHandler {
            id: imageHoverHandler
            cursorShape: Qt.PointingHandCursor
        }
        ToolTip.visible: imageHoverHandler.hovered
        ToolTip.text: room && eventId ? room.fileSource(eventId) : ""
        ToolTip.delay: Application.styleHints.mousePressAndHoldInterval

        TapHandler {
            acceptedButtons: Qt.LeftButton
            onTapped: {
                content.openOnFinished = true
                content.openExternally()
            }
        }
        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: controller.showMenu(index, textFieldImpl.hoveredLink,
                                          textFieldImpl.selectedText, showingDetails)
        }

        Component.onCompleted:
            if (visible && content.autoload && !content.downloaded
                    && !(progressInfo && progressInfo.isUpload))
                room.downloadFile(eventId)
    }

}
