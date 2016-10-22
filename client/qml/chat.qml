import QtQuick 2.2
import QtQuick.Controls 1.0
import QtQuick.Layouts 1.1

Rectangle {
    id: root

    SystemPalette { id: defaultPalette; colorGroup: SystemPalette.Active }
    SystemPalette { id: disabledPalette; colorGroup: SystemPalette.Disabled }

    color: defaultPalette.base

    signal getPreviousContent()

    function scrollToBottom() {
        chatView.positionViewAtEnd();
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
        property bool wasAtEndY: true

        function aboutToBeInserted() {
            wasAtEndY = nowAtYEnd;
            console.log("aboutToBeInserted! nowAtYEnd=" + nowAtYEnd);
        }

        function rowsInserted() {
            if( wasAtEndY )
            {
                root.scrollToBottom();
            } else  {
                console.log("was not at end, not scrolling");
            }
        }

        Component.onCompleted: {
            console.log("onCompleted");
            model.rowsAboutToBeInserted.connect(aboutToBeInserted);
            model.rowsInserted.connect(rowsInserted);
        }

        section {
            property: "date"
            labelPositioning: ViewSection.InlineLabels | ViewSection.CurrentLabelAtStart

            delegate: Rectangle {
                width:parent.width
                height: childrenRect.height
                color: defaultPalette.window
                Label { text: section.toLocaleString("dd.MM.yyyy") }
            }
        }

        onContentYChanged: {
            if( (this.contentY - this.originY) < 5 )
            {
                console.log("get older content!");
                root.getPreviousContent()
            }

        }
    }

    Slider {
        id: chatViewScroller
        orientation: Qt.Vertical
        anchors.horizontalCenter: chatView.right
        anchors.verticalCenter: chatView.verticalCenter
        height: chatView.height / 2

        value: -chatView.verticalVelocity / chatView.height
        maximumValue: 10.0
        minimumValue: -10.0

        activeFocusOnPress: false
        activeFocusOnTab: false

        onPressedChanged: {
            if (!pressed)
                value = 0
        }

        onValueChanged: {
            if (pressed && value)
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
            width: parent.width
            height: childrenRect.height

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
                            selectByMouse: true; readOnly: true; font: timelabel.font;
                            textFormat: contentType == "text/html" ? TextEdit.RichText
                                                                   : TextEdit.PlainText;
                            text: eventType != "image" ? content : ""
                            height: eventType != "image" ? implicitHeight : 0
                            wrapMode: Text.Wrap; width: parent.width
                            color: message.textColor

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.IBeamCursor
                                acceptedButtons: Qt.NoButton
                            }
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
                visible: messageModel.lastReadId === eventId
                color: message.textColor
                width: parent.width
                height: 1
                anchors.bottom: message.bottom
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
        id: scrollindicator;
        opacity: chatView.nowAtYEnd ? 0 : 0.5
        color: defaultPalette.text
        height: 30;
        radius: height/2;
        width: height;
        anchors.left: parent.left;
        anchors.bottom: parent.bottom;
        anchors.leftMargin: width/2;
        anchors.bottomMargin: chatView.nowAtYEnd ? -height : height/2;
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
