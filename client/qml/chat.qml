import QtQuick 2.2
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.0
import QtQuick.Layouts 1.1
import QMatrixClient 1.0

Rectangle {
    id: root

    Settings {
        id: settings
        readonly property bool condense_chat: value("UI/condense_chat", false)
        readonly property bool show_noop_events: value("UI/show_noop_events")
        readonly property bool autoload_images: value("UI/autoload_images", true)
        readonly property string highlight_color: value("UI/highlight_color", "orange")
        readonly property string render_type: value("UI/Fonts/render_type", "NativeRendering")
    }
    SystemPalette { id: defaultPalette; colorGroup: SystemPalette.Active }
    SystemPalette { id: disabledPalette; colorGroup: SystemPalette.Disabled }

    color:  defaultPalette.base

    function humanSize(bytes)
    {
        if (bytes < 4000)
            return qsTr("%1 bytes").arg(bytes)
        bytes = Math.round(bytes / 100) / 10
        if (bytes < 2000)
            return qsTr("%1 KB").arg(bytes)
        bytes = Math.round(bytes / 100) / 10
        if (bytes < 2000)
            return qsTr("%1 MB").arg(bytes)
        return qsTr("%1 GB").arg(Math.round(bytes / 100) / 10)
    }

    ListView {
        id: chatView
        anchors.fill: parent

        model: messageModel
        delegate: messageDelegate
        flickableDirection: Flickable.VerticalFlick
        flickDeceleration: 9001
        boundsBehavior: Flickable.StopAtBounds
        pixelAligned: true
        // FIXME: atYEnd is glitchy on Qt 5.2.1
        property bool nowAtYEnd: contentY - originY + height >= contentHeight
        property bool stickToBottom: true
        property int largestVisibleIndex: indexAt(1, contentY + height - 1)

        function ensurePreviousContent() {
            // Check whether we're about to bump into the ceiling in 2 seconds
            var curVelocity = verticalVelocity // Snapshot the current speed
            if( curVelocity < 0 && contentY + curVelocity*2 < originY)
            {
                // Request the amount of messages enough to scroll at this
                // rate for 3 more seconds
                var avgHeight = contentHeight / count
                model.room.getPreviousContent(-curVelocity*3 / avgHeight);
            }
        }

        function onModelAboutToReset() {
            contentYChanged.disconnect(ensurePreviousContent)
            console.log("Chat: getPreviousContent disabled")
        }

        function onModelReset() {
            if (model.room)
            {
                if (contentY < originY + 10)
                    model.room.getPreviousContent(100)
                contentYChanged.connect(ensurePreviousContent)
                console.log("Chat: getPreviousContent enabled")
                scrollToBottom()
            }
        }

        Timer {
            id: scrollTimer
            interval: 0
            onTriggered: chatView.reallyScrollToBottom()
        }

        function scrollToBottom() {
            stickToBottom = true
            scrollTimer.running = true
            reallyScrollToBottom()
        }

        function reallyScrollToBottom() {
            if (stickToBottom && !nowAtYEnd)
            {
                positionViewAtEnd()
                scrollToBottom()
            }
        }

        function rowsInserted() {
            if( stickToBottom )
                scrollToBottom()
        }

        Component.onCompleted: {
            console.log("onCompleted")
            model.modelAboutToBeReset.connect(onModelAboutToReset)
            model.modelReset.connect(onModelReset)
            model.rowsInserted.connect(rowsInserted)
        }

        section {
            property: "section"
            labelPositioning: ViewSection.InlineLabels | ViewSection.CurrentLabelAtStart

            delegate: Rectangle {
                width: root.width
                height: childrenRect.height
                color: defaultPalette.window
                Label { text: section.toLocaleString("dd.MM.yyyy") }
            }
        }

        onHeightChanged: {
            if( stickToBottom )
                scrollToBottom()
        }

        onContentHeightChanged: {
            if( stickToBottom )
                scrollToBottom()
        }

        onMovementStarted: {
            stickToBottom = false
        }

        onMovementEnded: {
            stickToBottom = nowAtYEnd
        }

        // Scrolling controls

        Slider {
            id: chatViewScroller
            orientation: Qt.Vertical
            height: parent.height
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            z: 3

            style: SliderStyle {
                // Width and height are swapped below because SliderStyle assumes
                // a horizontal slider - but anchors are not :-\
                groove: Rectangle {
                    color: defaultPalette.window
                    border.color: defaultPalette.midlight
                    implicitHeight: 8
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: parent.right
                        implicitHeight: 2
                        width: chatView.height *
                           ((chatView.largestVisibleIndex + 1) / chatView.count)

                        color: defaultPalette.highlight
                    }
                }
                handle: Rectangle {
                    anchors.centerIn: parent
                    color: defaultPalette.button
                    border.color: defaultPalette.buttonText
                    border.width: 1
                    implicitWidth: 14
                    implicitHeight: 8
                    visible: chatView.count > 0
                }
            }

            maximumValue: 10.0
            minimumValue: -10.0

            activeFocusOnPress: false
            activeFocusOnTab: false
            // wheelEnabled: false // Only available in QQC 1.6, Qt 5.10

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                onWheel: { } // FIXME: propagate wheel event to chatView
            }

            onPressedChanged: {
                if (pressed)
                    parent.stickToBottom = false
                else
                    value = 0
            }

            onValueChanged: {
                if (value)
                    parent.flick(0, parent.height * value)
            }
            Component.onCompleted: {
                // This will cause continuous scrolling while the scroller is out of 0
                parent.flickEnded.connect(chatViewScroller.valueChanged)
            }
        }
        Label {
            anchors.right: chatViewScroller.left
            anchors.top: parent.top
            visible: chatView.largestVisibleIndex < chatView.count - 5
            z: 3 // On top of ListView sections that have z=2
            text: qsTr("%1 events back from now (%2 cached)")
                    .arg(chatView.count - chatView.largestVisibleIndex - 1)
                    .arg(chatView.count)
        }
    }

    Component {
        id: messageDelegate

        Item {
            id: delegateItem
            width: root.width - chatViewScroller.width
            height: hidden ? 0 : childrenRect.height
            visible: !hidden

            readonly property bool redacted: marks == "redacted"
            readonly property bool hidden: marks == "hidden" && !settings.show_noop_events
            readonly property string textColor:
                redacted ? disabledPalette.text :
                highlight ? settings.highlight_color :
                (["state", "notice", "other"].indexOf(eventType) >= 0) ?
                        disabledPalette.text : defaultPalette.text

            // A message is considered shown if its bottom is within the
            // viewing area of the timeline.
            readonly property bool shown:
                y + message.height - 1 > chatView.contentY &&
                y + message.height - 1 < chatView.contentY + chatView.height

            onShownChanged:
                controller.onMessageShownChanged(eventId, shown)

            Component.onCompleted: {
                if (shown)
                    shownChanged(true);
            }

            ListView.onAdd: NumberAnimation {
                target: delegateItem; properties: "opacity"
                from: 0.01; to: 1; duration: 300; easing.type: Easing.InOutQuad
            }

            Column {
                id: fullMessage
                width: parent.width

                Item {
                    id: message
                    width: parent.width
                    height: childrenRect.height

                    Label {
                        id: timelabel
                        anchors.top: textField.top
                        anchors.left: parent.left

                        color: disabledPalette.text
                        renderType: settings.render_type

                        text: "<" + time.toLocaleTimeString(
                                        Qt.locale(), Locale.ShortFormat) + ">"
                    }
                    Label {
                        id: authorLabel
                        width: 120
                        anchors.top: textField.top
                        anchors.left: timelabel.right
                        anchors.leftMargin: 3
                        horizontalAlignment: if( ["other", "emote", "state"]
                                                     .indexOf(eventType) >= 0 )
                                             { Text.AlignRight }
                        elide: Text.ElideRight

                        color: textColor
                        renderType: settings.render_type

                        text: eventType == "state" || eventType == "emote" ?
                                  "* " + author :
                              eventType != "other" ? author : "***"

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                controller.insertMention(author)
                                controller.focusInput()
                            }
                        }
                    }
                    TextEdit {
                        id: textField
                        anchors.left: authorLabel.right
                        anchors.leftMargin: 3
                        anchors.right: showDetailsButton.left
                        anchors.rightMargin: 3

                        selectByMouse: true;
                        readOnly: true;
                        textFormat: TextEdit.RichText
                        text: display
                        wrapMode: Text.Wrap;
                        color: textColor
                        renderType: settings.render_type

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.IBeamCursor
                            acceptedButtons: Qt.NoButton
                        }

                        // TODO: In the code below, links should be resolved
                        // with Qt.resolvedLink, once we figure out what
                        // to do with relative URLs (note: www.google.com
                        // is a relative URL, https://www.google.com is not).
                        // Instead of Qt.resolvedUrl (and, most likely,
                        // QQmlAbstractUrlInterceptor to convert URLs)
                        // we might just prefer to do the whole resolving
                        // in C++.
                        onHoveredLinkChanged:
                            controller.showStatusMessage(hoveredLink)

                        onLinkActivated: Qt.openUrlExternally(link)
                    }
                    Loader {
//                            asynchronous: true // https://bugreports.qt.io/browse/QTBUG-50992
                        id: contentLoader
                        active: eventType == "file" || eventType == "image"
                        visible: status == Loader.Ready
                        anchors.top: textField.bottom
                        anchors.left: textField.left
                        anchors.right: textField.right

                        sourceComponent:
                            eventType == "file" ? downloadControls :
                            eventType == "image" ? imageField : undefined
                    }
                    ToolButton {
                        id: showDetailsButton
                        anchors.top: textField.top
                        anchors.right: parent.right
                        height:
                            !settings.condense_chat ? implicitHeight :
                            Math.min(implicitHeight,
                                textField.visible ? textField.height :
                                                    contentLoader.height)

                        text: "..."

                        action: Action {
                            id: showDetails

                            tooltip: "Show details and actions"
                            checkable: true
                            onCheckedChanged: {
                                if (checked)
                                    chatView.stickToBottom = false
                                else
                                    chatView.stickToBottom = chatView.nowAtYEnd
                            }
                        }
                    }
                }
                Loader {
//                    asynchronous: true // https://bugreports.qt.io/browse/QTBUG-50992
                    visible: status == Loader.Ready
                    width: parent.width
                    height: childrenRect.height

                    sourceComponent: showDetails.checked ? detailsArea : undefined
                }
            }
            Rectangle {
                id: readMarker
                color: defaultPalette.highlight
                width: messageModel.room.readMarkerEventId === eventId &&
                           root.width
                height: 1
                anchors.bottom: fullMessage.bottom
                Behavior on width {
                    NumberAnimation { duration: 500; easing.type: Easing.OutQuad }
                }
            }

            // Components loaded on demand

            function openSavedFile()
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

            property bool openOnFinished: false
            readonly property bool downloaded: progressInfo &&
                                      progressInfo.completed || false

            onDownloadedChanged: {
                if (downloaded && openOnFinished)
                    openSavedFile()
            }

            Component {
                id: imageField
                Column {
                    width: parent.width

                    Image {
                        sourceSize.height: progressInfo.active ?
                                               content.info.h :
                                               content.info.thumbnail_info.h
                        sourceSize.width: progressInfo.active ?
                                              content.info.w :
                                              content.info.thumbnail_info.w
                        width: parent.width
                        height: sourceSize.height *
                                (width < sourceSize.width ?
                                     width / sourceSize.width : 1)
                        fillMode: Image.PreserveAspectFit
                        source: downloaded ? progressInfo.localPath :
                                           "image://mtx/" + content.thumbnailMediaId

                        Component.onCompleted:
                            if (settings.autoload_images && !downloaded)
                                messageModel.room.downloadFile(eventId)
                    }

                    RowLayout {
                        width: parent.width
                        spacing: 2

                        Button {
                            text: "Cancel downloading"
                            visible: progressInfo.active && !downloaded
                            onClicked: {
                                messageModel.room.cancelFileTransfer(eventId)
                            }
                        }

                        Button {
                            text: "Open in image viewer"
                            onClicked: {
                                if (downloaded)
                                    openSavedFile()
                                else
                                {
                                    openOnFinished = true
                                    messageModel.room.downloadFile(eventId)
                                }
                            }
                        }
                        Button {
                            text: "Download full size"
                            visible: !settings.autoload_images &&
                                     !progressInfo.active

                            onClicked: messageModel.room.downloadFile(eventId)
                        }
                        Button {
                            text: "Save as..."
                            onClicked: controller.saveFileAs(eventId)
                        }
                    }
                }
            }

            Component {
                id: downloadControls

                Column {
                    Item {
                        width: parent.width
                        height: downloadInfo.height
                        ProgressBar {
                            id: downloadProgress
                            value: progressInfo.progress / progressInfo.total
                            indeterminate: progressInfo.progress < 0
                            visible: progressInfo.active && !downloaded
                            anchors.fill: downloadInfo
                        }
                        TextEdit {
                            id: downloadInfo
                            selectByMouse: true;
                            readOnly: true;
                            font: timelabel.font
                            color: textColor
                            renderType: settings.render_type
                            text:
                                qsTr("Size: %1, declared type: %2%3")
                                .arg(humanSize(content.info.size))
                                .arg(content.info.mimetype)
                                .arg(downloaded
                                     ? qsTr(" (downloaded%1)")
                                       .arg(progressInfo.filePath ?
                                            " as " + progressInfo.filePath : "")
                                     : "")
                            textFormat: TextEdit.PlainText
                            wrapMode: Text.Wrap;
                            width: parent.width
                        }
                    }
                    RowLayout {
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
                            onClicked: {
                                messageModel.room.cancelFileTransfer(eventId)
                            }
                        }

                        Button {
                            text: "Open"
                            visible: !openOnFinishedFlag.visible
                            onClicked: {
                                if (downloaded)
                                    openSavedFile()
                                else
                                {
                                    openOnFinished = true
                                    messageModel.room.downloadFile(eventId)
                                }
                            }
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
            }

            Component {
                id: detailsArea

                Rectangle {
                    height: childrenRect.height
                    radius: 5

                    color: defaultPalette.button
                    border.color: defaultPalette.mid

                    readonly property url evtLink:
                        "https://matrix.to/#/" + messageModel.room.id + "/" + eventId
                    property string sourceText: toolTip

                    Item {
                        id: detailsHeader
                        width: parent.width
                        height: childrenRect.height
                        anchors.top: parent.top

                        TextEdit {
                            text: "<" + time.toLocaleString(Qt.locale(), Locale.ShortFormat) + ">"
                            font.bold: true
                            renderType: settings.render_type
                            readOnly: true
                            selectByKeyboard: true; selectByMouse: true

                            anchors.left: parent.left
                            anchors.leftMargin: 3
                            anchors.verticalCenter: copyLinkButton.verticalCenter
                            z: 1
                        }
                        TextEdit {
                            text: "<a href=\"" + evtLink + "\">"+ eventId + "</a>"
                            textFormat: Text.RichText
                            font.bold: true
                            renderType: settings.render_type
                            horizontalAlignment: Text.AlignHCenter
                            readOnly: true
                            selectByKeyboard: true; selectByMouse: true

                            width: parent.width
                            anchors.top: copyLinkButton.bottom

                            onLinkActivated: Qt.openUrlExternally(link)

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: parent.hoveredLink ?
                                                 Qt.PointingHandCursor :
                                                 Qt.IBeamCursor
                                acceptedButtons: Qt.NoButton
                            }
                        }
                        Button {
                            id: redactButton

                            text: "Redact"

                            anchors.right: copyLinkButton.left
                            z: 1

                            onClicked: {
                                messageModel.room.redactEvent(eventId)
                                showDetails.checked = false
                            }
                        }
                        Button {
                            id: copyLinkButton

                            text: "Copy link to clipboard"

                            anchors.right: parent.right
                            z: 1

                            onClicked: {
                                permalink.selectAll()
                                permalink.copy()
                                showDetails.checked = false
                            }
                        }
                        TextEdit {
                            id: permalink
                            text: evtLink
                            renderType: settings.render_type
                            width: 0; height: 0; visible: false
                        }
                    }

                    TextArea {
                        text: sourceText;
                        textFormat: Text.PlainText
                        readOnly: true;
                        font.family: "Monospace"
                        // FIXME: make settings.render_type an integer (but store as string to stay human-friendly)
//                        style: TextAreaStyle {
//                            renderType: settings.render_type
//                        }
                        selectByKeyboard: true; selectByMouse: true;

                        width: parent.width
                        anchors.top: detailsHeader.bottom
                    }
                }
            }
        }
    }

    Rectangle {
        id: scrollindicator
        opacity: chatView.nowAtYEnd ? 0 : 0.5
        color: defaultPalette.text
        height: 30
        radius: height/2
        width: height
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: width/2
        anchors.bottomMargin: chatView.nowAtYEnd ? -height : height/2
        Behavior on opacity {
            NumberAnimation { duration: 300 }
        }
        Behavior on anchors.bottomMargin {
            NumberAnimation { duration: 300 }
        }
        Image {
            anchors.fill: parent
            source: "qrc:///scrolldown.svg"
        }
        MouseArea {
            anchors.fill: parent
            onClicked: chatView.scrollToBottom()
            cursorShape: Qt.PointingHandCursor
        }
    }
}
