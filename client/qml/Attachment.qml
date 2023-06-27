import QtQuick 2.0
import Quotient 1.0

Item {
    width: parent.width
    height: visible ? childrenRect.height : 0

    required property bool openOnFinished
    readonly property bool downloaded: progressInfo &&
                !progressInfo.isUpload && progressInfo.completed
    signal opened

    onDownloadedChanged: {
        if (downloaded && openOnFinished)
            openLocalFile()
    }

    function openExternally()
    {
        if (progressInfo.localPath.toString() || downloaded)
            openLocalFile()
        else
            room.downloadFile(eventId)
    }

    function openLocalFile()
    {
        if (!Qt.openUrlExternally(progressInfo.localPath)) {
            controller.showStatusMessage(
                "Couldn't determine how to open the file, " +
                "opening its folder instead", 5000)

            if (!Qt.openUrlExternally(progressInfo.localDir)) {
                controller.showStatusMessage(
                    "Couldn't determine how to open the file or its folder.",
                    5000)
                return;
            }
        }
        opened()
    }

    Connections {
        target: controller
        function onOpenExternally(currentIndex) {
            if (currentIndex === index)
                openExternally()
        }
    }
}
