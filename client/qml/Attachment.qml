import QtQuick 2.0
import Quotient 1.0

Item {
    width: parent.width
    height: visible ? childrenRect.height : 0

    property bool openOnFinished: false
    readonly property bool downloaded: progressInfo &&
                !progressInfo.isUpload && progressInfo.completed

    onDownloadedChanged: {
        if (downloaded && openOnFinished)
            openLocalFile()
    }

    function openExternally()
    {
        if (progressInfo.localPath.toString() || downloaded)
            openLocalFile()
        else
        {
            openOnFinished = true
            room.downloadFile(eventId)
        }
    }

    function openLocalFile()
    {
        if (Qt.openUrlExternally(progressInfo.localPath))
            return;

        controller.showStatusMessage(
            "Couldn't determine how to open the file, " +
            "opening its folder instead", 5000)

        if (Qt.openUrlExternally(progressInfo.localDir))
            return;

        controller.showStatusMessage(
            "Couldn't determine how to open the file or its folder.",
            5000)
    }

    Connections {
        target: controller
        onOpenExternally: if (currentIndex === index) openExternally()
    }
}
