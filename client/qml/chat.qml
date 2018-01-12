import QtQuick 2.2
import QtQuick.Controls 1.0
import QtQuick.Controls.Styles 1.0
import QtQuick.Layouts 1.1
import QMatrixClient 1.0

Rectangle {
    id: root

    Settings {
        id: settings
        property bool condense_chat: value("UI/condense_chat", false)
        property bool show_noop_events: value("UI/show_noop_events")
    }
    SystemPalette { id: defaultPalette; colorGroup: SystemPalette.Active }
    SystemPalette { id: disabledPalette; colorGroup: SystemPalette.Disabled }

    Timer {
        id: scrollTimer
        interval: 0
        onTriggered: reallyScrollToBottom()
    }

    color:  defaultPalette.base

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
                if (contentHeight <= height)
                    model.room.getPreviousContent(100)
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
            property bool redacted: marks == "redacted"
            property bool hidden: marks == "hidden" && !settings.show_noop_events

            color: defaultPalette.base
            width: root.width - chatViewScroller.width
            height: hidden ? 0 : childrenRect.height
            visible: !hidden

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
                        if (redacted) disabledPalette.text
                        else if (highlight) decoration
                        else if (["state", "notice", "other"]
                                 .indexOf(eventType) >= 0) disabledPalette.text
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
                    id: contentRect
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
                            width: parent.width
                            sourceSize.width: parent.width
                            source: eventType == "image" ? content : ""
                        }
                    }
                }
                ToolButton {
                    id: showDetailsButton

                    text: "..."
                    Layout.maximumHeight: settings.condense_chat ?
                                          contentRect.height : implicitHeight
                    Layout.alignment: Qt.AlignTop


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
                asynchronous: true
                visible: status == Loader.Ready
                anchors.left: message.left
                anchors.right: message.right
                anchors.rightMargin: showDetailsButton.width
                height: childrenRect.height

                property string evtId: eventId
                property url evtLink:
                    "https://matrix.to/#/" + messageModel.room.id + "/" + eventId
                property var timestamp: time
                property var showDetails: showDetails
                property string sourceText: toolTip

                sourceComponent: showDetails.checked ? detailsArea : undefined
            }
            Rectangle {
                id: readMarker
                color: defaultPalette.highlight
                width: messageModel.room.readMarkerEventId === eventId &&
                           root.width
                height: 1
                anchors.bottom: message.bottom
                Behavior on width {
                    NumberAnimation { duration: 500; easing.type: Easing.OutQuad }
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

            Item {
                id: header
                width: parent.width
                height: childrenRect.height
                anchors.top: parent.top

                TextEdit {
                    text: "<" + timestamp.toLocaleString(Qt.locale(), Locale.ShortFormat) + ">"
                    font.bold: true
                    readOnly: true
                    selectByKeyboard: true; selectByMouse: true

                    anchors.left: parent.left
                    anchors.leftMargin: 3
                    anchors.verticalCenter: copyLinkButton.verticalCenter
                    z: 1
                }
                TextEdit {
                    text: "<a href=\""+ evtLink + "\">"+ evtId + "</a>"
                    textFormat: Text.RichText
                    font.bold: true
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
                        messageModel.room.redactEvent(evtId)
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
                    width: 0; height: 0; visible: false
                }
            }

            TextArea {
                text: sourceText;
                textFormat: Text.PlainText
                readOnly: true;
                font.family: "Monospace"
                selectByKeyboard: true; selectByMouse: true;

                width: parent.width
                anchors.top: header.bottom
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
            onClicked: root.scrollToBottom()
            cursorShape: Qt.PointingHandCursor
        }
    }
}
