import QtQuick 2.2
import QtQuick.Controls 1.0
import QtQuick.Controls.Styles 1.0
import QtQuick.Layouts 1.1

Item {
    id: root

    SystemPalette { id: defaultPalette; colorGroup: SystemPalette.Active }
    SystemPalette { id: disabledPalette; colorGroup: SystemPalette.Disabled }

    Timer {
        id: scrollTimer
        interval: 0
        onTriggered: reallyScrollToBottom()
    }

    function reallyScrollToBottom() {
        if (chatView.stickToBottom && !chatView.nowAtYEnd)
        {
            chatView.positionViewAtEnd()
            scrollToBottom()
        }
    }

    function scrollToBottom() {
        chatView.stickToBottom = true
        scrollTimer.running = true
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
                contentYChanged.connect(ensurePreviousContent)
                console.log("Chat: getPreviousContent enabled")
            }
        }

        function rowsInserted() {
            if( stickToBottom )
                root.scrollToBottom()
        }

        Component.onCompleted: {
            console.log("onCompleted")
            model.modelAboutToBeReset.connect(onModelAboutToReset)
            model.modelReset.connect(onModelReset)
            model.rowsInserted.connect(rowsInserted)
        }

        section {
            property: "date"
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
                root.scrollToBottom()
        }

        onContentHeightChanged: {
            if( stickToBottom )
                root.scrollToBottom()
        }

        onMovementStarted: {
            stickToBottom = false
        }

        onMovementEnded: {
            stickToBottom = nowAtYEnd
        }
    }

    Slider {
        id: chatViewScroller
        orientation: Qt.Vertical
        anchors.right: chatView.right
        anchors.verticalCenter: chatView.verticalCenter

        style: SliderStyle {
            // Width and height are swapped below because SliderStyle assumes
            // a horizontal slider
            groove: Rectangle {
                implicitHeight: 3
                implicitWidth: chatView.height / 2
                anchors.horizontalCenter: parent.horizontalCenter
                color: defaultPalette.highlight
                radius: 3
            }
            handle: Rectangle {
                anchors.centerIn: parent
                color: defaultPalette.button
                border.color: defaultPalette.buttonText
                border.width: 1
                implicitWidth: 21
                implicitHeight: 9
                radius: 9
            }
        }

        maximumValue: 10.0
        minimumValue: -10.0

        activeFocusOnPress: false
        activeFocusOnTab: false

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            onWheel: { } // Disable wheel on the slider
        }

        onPressedChanged: {
            if (pressed)
                chatView.stickToBottom = false
            else
                value = 0
        }

        onValueChanged: {
            if (value)
                chatView.flick(0, chatView.height * value)
        }
        Component.onCompleted: {
            // This will cause continuous scrolling while the scroller is out of 0
            chatView.flickEnded.connect(chatViewScroller.valueChanged)
        }
    }

    Component {
        id: messageDelegate

        Rectangle {
            color: defaultPalette.base
            width: root.width - chatViewScroller.width
            height: childrenRect.height

            // A message is considered shown if its bottom is within the
            // viewing area of the timeline.
            property bool shown:
                y + message.height - 1 > chatView.contentY &&
                y + message.height - 1 < chatView.contentY + chatView.height

            Component.onCompleted: {
                if (shown)
                    shownChanged(true);
            }

            onShownChanged:
                controller.onMessageShownChanged(eventId, shown)

            RowLayout {
                id: message
                width: parent.width
                spacing: 3

                property string textColor:
                        if (highlight) decoration
                        else if (eventType == "state" || eventType == "other") disabledPalette.text
                        else defaultPalette.text

                Label {
                    Layout.alignment: Qt.AlignTop
                    id: timelabel
                    text: "<" + time.toLocaleTimeString(Qt.locale(), Locale.ShortFormat) + ">"
                    color: disabledPalette.text
                }
                Label {
                    Layout.alignment: Qt.AlignTop | Qt.AlignLeft
                    Layout.preferredWidth: 120
                    elide: Text.ElideRight
                    text: eventType == "state" || eventType == "emote" ? "* " + author :
                          eventType != "other" ? author : "***"
                    horizontalAlignment: if( ["other", "emote", "state"]
                                                 .indexOf(eventType) >= 0 )
                                         { Text.AlignRight }
                    color: message.textColor
                }
                Rectangle {
                    color: defaultPalette.base
                    Layout.fillWidth: true
                    Layout.minimumHeight: childrenRect.height
                    Layout.alignment: Qt.AlignTop | Qt.AlignLeft

                    Column {
                        spacing: 0
                        width: parent.width

                        TextEdit {
                            id: contentField
                            selectByMouse: true; readOnly: true; font: timelabel.font
                            textFormat: contentType == "text/html" ? TextEdit.RichText
                                                                   : TextEdit.PlainText
                            text: eventType != "image" ? content : ""
                            height: eventType != "image" ? implicitHeight : 0
                            wrapMode: Text.Wrap; width: parent.width
                            color: message.textColor

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

                            onLinkActivated: {
                                Qt.openUrlExternally(link)
                            }
                        }
                        Image {
                            id: imageField
                            fillMode: Image.PreserveAspectFit
                            width: eventType == "image" ? parent.width : 0

                            sourceSize: eventType == "image" ? "500x500" : "0x0"
                            source: eventType == "image" ? content : ""
                        }
                        Loader {
                            asynchronous: true
                            visible: status == Loader.Ready
                            width: parent.width
                            property string sourceText: toolTip

                            sourceComponent: showSource.checked ? sourceArea : undefined
                        }
                    }
                }
                ToolButton {
                    id: showSourceButton
                    text: "..."
                    Layout.alignment: Qt.AlignTop

                    action: Action {
                        id: showSource

                        tooltip: "Show source"
                        checkable: true
                    }
                }
            }
            Rectangle {
                id: readMarker
                color: defaultPalette.highlight
                width: messageModel.room.readMarkerEventId === eventId ? parent.width : 0
                height: 1
                anchors.bottom: message.bottom
                anchors.horizontalCenter: message.horizontalCenter
                Behavior on width {
                    NumberAnimation { duration: 500; easing.type: Easing.OutQuad }
                }
            }
        }
    }

    Component {
        id: sourceArea

        TextArea {
            selectByMouse: true; readOnly: true; font.family: "Monospace"
            text: sourceText
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
            onClicked: root.scrollToBottom()
            cursorShape: Qt.PointingHandCursor
        }
    }
}
