import QtQuick 2.0
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.1

DownloadableContent {
    TextEdit {
        id: downloadInfo
        width: parent.width

        selectByMouse: true;
        readOnly: true;
        font: timelabel.font
        color: textColor
        renderType: settings.render_type
        text: qsTr("Size: %1, declared type: %2")
              .arg(humanSize(content.info.size))
              .arg(content.info.mimetype)
              + (!downloaded ? "" :
                  qsTr(" (downloaded to %1)").arg(progressInfo.localPath))
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
        id: downloadProgress
        visible: progressInfo.active && !downloaded
        anchors.fill: downloadInfo

        value: progressInfo.progress / progressInfo.total
        indeterminate: progressInfo.progress < 0
    }
    RowLayout {
        anchors.top: downloadInfo.bottom
        width: parent.width
        spacing: 2

        CheckBox {
            id: openOnFinishedFlag
            text: "Open after downloading"
            visible: downloadProgress.visible
            checked: openOnFinished
        }
        Button {
            text: "Cancel"
            visible: downloadProgress.visible
            onClicked: room.cancelFileTransfer(eventId)
        }

        Button {
            text: "Open"
            visible: !openOnFinishedFlag.visible
            onClicked: downloadAndOpen()
        }
        Button {
            text: "Save as..."
            visible: !progressInfo.active
            onClicked: controller.saveFileAs(eventId)
        }
        Button {
            text: "Open folder"
            visible: progressInfo.active
            onClicked:
                Qt.openUrlExternally(progressInfo.localDir)
        }
    }
}
