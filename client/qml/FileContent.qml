import QtQuick 2.0
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.1

Attachment {
    TextEdit {
        id: fileTransferInfo
        width: parent.width

        selectByMouse: true;
        readOnly: true;
        font: timelabel.font
        color: textColor
        renderType: settings.render_type
        text: qsTr("Size: %1, declared type: %2")
              .arg(content.info ? humanSize(content.info.size) : "")
              .arg(content.info ? content.info.mimetype : "unknown")
              + (progressInfo && progressInfo.uploading
                 ? (progressInfo.completed
                    ? qsTr(" (uploaded from %1)", "%1 is a local file name")
                    : qsTr(" (being uploaded from %1)", "%1 is a local file name"))
                   .arg(progressInfo.localPath)
                 : downloaded
                 ? qsTr(" (downloaded to %1)", "%1 is a local file name")
                   .arg(progressInfo.localPath)
                 : "")
        textFormat: TextEdit.PlainText
        wrapMode: Text.Wrap;

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            hoverEnabled: true
            cursorShape: Qt.IBeamCursor

            onContainsMouseChanged:
                controller.showStatusMessage(containsMouse ?
                                                room.urlToDownload(eventId) : "")
        }
    }
    ProgressBar {
        id: transferProgress
        visible: progressInfo.active && !progressInfo.completed
        anchors.fill: fileTransferInfo

        value: progressInfo.progress / progressInfo.total
        indeterminate: progressInfo.progress < 0
    }
    RowLayout {
        anchors.top: fileTransferInfo.bottom
        width: parent.width
        spacing: 2

        CheckBox {
            id: openOnFinishedFlag
            text: qsTr("Open after downloading")
            visible: progressInfo.downloading && transferProgress.visible
            checked: openOnFinished
        }
        Button {
            text: qsTr("Cancel")
            // Uploading has its own "Discard" button
            visible: progressInfo.downloading && transferProgress.visible
            onClicked: room.cancelFileTransfer(eventId)
        }
        Button {
            text: qsTr("Save as...")
            visible: !transferProgress.visible
            onClicked: controller.saveFileAs(eventId)
        }

        Button {
            text: qsTr("Open")
            visible: !openOnFinishedFlag.visible
            onClicked: downloadAndOpen()
        }
        Button {
            text: qsTr("Open folder")
            visible: progressInfo.active
            onClicked:
                Qt.openUrlExternally(progressInfo.localDir)
        }
    }
}
