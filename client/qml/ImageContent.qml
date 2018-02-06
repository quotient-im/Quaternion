import QtQuick 2.0
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.1

DownloadableContent {
    property size imageSourceSize
    property url imageSource
    property bool autoload

    readonly property real sizeFactor:
        Math.min((chatView.height - buttons.height) / imageSourceSize.height * 0.9,
                 Math.min(width / imageSourceSize.width, 1))

    Image {
        id: imageContent
        width: parent.width
        height: imageSourceSize.height * sizeFactor
        fillMode: Image.PreserveAspectFit

        source: imageSource
        sourceSize: imageSourceSize

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor

            onContainsMouseChanged:
                controller.showStatusMessage(containsMouse ?
                                                room.urlToDownload(eventId) : "")
            onClicked: downloadAndOpen()
        }

        Component.onCompleted:
            if (visible && autoload && !downloaded)
                room.downloadFile(eventId)
    }

    RowLayout {
        id: buttons
        anchors.top: imageContent.bottom
        width: parent.width
        spacing: 2

        Button {
            text: "Cancel downloading"
            visible: progressInfo.active && !downloaded
            onClicked: room.cancelFileTransfer(eventId)
        }

        Button {
            text: "Open in image viewer"
            onClicked: downloadAndOpen()
        }
        Button {
            text: "Download full size"
            visible: !autoload && !progressInfo.active

            onClicked: room.downloadFile(eventId)
        }
        Button {
            text: "Save as..."
            onClicked: controller.saveFileAs(eventId)
        }
    }
}
