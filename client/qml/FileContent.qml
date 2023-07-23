import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.1

Attachment {
    openOnFinished: openButton.checked

    TextEdit {
        id: fileTransferInfo
        width: parent.width

        selectByMouse: true;
        readOnly: true;
        font: timelabel.font
        color: foreground
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

        HoverHandler {
            id: fileContentHoverHandler
            cursorShape: Qt.IBeamCursor
        }
        ToolTip.visible: fileContentHoverHandler.hovered
        ToolTip.text: room.fileSource(eventId)

        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: controller.showMenu(index, textFieldImpl.hoveredLink,
                                          textFieldImpl.selectedText,
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

        TimelineItemToolButton {
            id: openButton
            text: progressInfo &&
                  !progressInfo.isUpload && transferProgress.visible
                  ? qsTr("Open after downloading") : qsTr("Open")
            checkable: !downloaded && !(progressInfo && progressInfo.isUpload)
            onClicked: { if (checked) openExternally() }
        }
        TimelineItemToolButton {
            text: qsTr("Cancel")
            visible: progressInfo && progressInfo.started
            onClicked: room.cancelFileTransfer(eventId)
        }
        TimelineItemToolButton {
            text: qsTr("Save as...")
            visible: !progressInfo ||
                     (!progressInfo.isUpload && !progressInfo.started)
            onClicked: controller.saveFileAs(eventId)
        }
        TimelineItemToolButton {
            text: qsTr("Open folder")
            visible: progressInfo && progressInfo.localDir
            onClicked:
                Qt.openUrlExternally(progressInfo.localDir)
        }
    }
    onOpened: openButton.checked = false
}
