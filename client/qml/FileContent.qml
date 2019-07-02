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
              + (progressInfo && progressInfo.isUpload
                 ? " (" + (progressInfo.completed
                    ? qsTr("uploaded from %1", "%1 is a local file name")
                    : qsTr("being uploaded from %1", "%1 is a local file name"))
                   .arg(progressInfo.localPath) + ')'
                 : downloaded
                 ? " (" + qsTr("downloaded to %1", "%1 is a local file name")
                          .arg(progressInfo.localPath) + ')'
                 : "")
        textFormat: TextEdit.PlainText
        wrapMode: Text.Wrap;

        TimelineMouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            hoverEnabled: true

            onContainsMouseChanged:
                controller.showStatusMessage(containsMouse
                                             ? room.fileSource(eventId) : "")
        }

        TimelineMouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            cursorShape: Qt.IBeamCursor

            onClicked: controller.showMenu(index, textFieldImpl.hoveredLink,
                showingDetails)
        }
    }
    ProgressBar {
        id: transferProgress
        visible: progressInfo && progressInfo.started
        anchors.fill: fileTransferInfo

        value: progressInfo ? progressInfo.progress / progressInfo.total : -1
        indeterminate: !progressInfo || progressInfo.progress < 0
    }
    RowLayout {
        anchors.top: fileTransferInfo.bottom
        width: parent.width
        spacing: 2

        CheckBox {
            id: openOnFinishedFlag
            text: qsTr("Open after downloading")
            visible: progressInfo &&
                     !progressInfo.isUpload && transferProgress.visible
            checked: openOnFinished
        }
        Button {
            text: qsTr("Cancel")
            visible: progressInfo && progressInfo.started
            onClicked: room.cancelFileTransfer(eventId)
        }
        Button {
            text: qsTr("Save as...")
            visible: !progressInfo ||
                     (!progressInfo.isUpload && !progressInfo.started)
            onClicked: controller.saveFileAs(eventId)
        }

        Button {
            text: qsTr("Open")
            visible: !openOnFinishedFlag.visible
            onClicked: openExternally()
        }
        Button {
            text: qsTr("Open folder")
            visible: progressInfo && progressInfo.localDir
            onClicked:
                Qt.openUrlExternally(progressInfo.localDir)
        }
    }
}
