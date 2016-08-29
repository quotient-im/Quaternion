import QtQuick 2.0
import QtQuick.Controls 1.0
import QtQuick.Layouts 1.1

Rectangle {
    id: root

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
        boundsBehavior: Flickable.StopAtBounds
        pixelAligned: true
        property bool wasAtEndY: true

        function aboutToBeInserted() {
            wasAtEndY = atYEnd;
            console.log("aboutToBeInserted! atYEnd=" + atYEnd);
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
            //positionViewAtEnd();
        }

        section {
            property: "date"
            labelPositioning: ViewSection.InlineLabels | ViewSection.CurrentLabelAtStart

            delegate: Rectangle {
                width:parent.width
                height: childrenRect.height
                color: "lightgrey"
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

        RowLayout {
            id: message
            width: parent.width
            spacing: 3

            Label {
                Layout.alignment: Qt.AlignTop
                id: timelabel
                text: "<" + time.toLocaleTimeString() + ">"
                color: "grey"
            }
            Label {
                Layout.alignment: Qt.AlignTop | Qt.AlignLeft
                Layout.preferredWidth: 120
                elide: Text.ElideRight
                text: eventType == "message" || eventType == "image" ? author : "***"
                horizontalAlignment: if( eventType == "other" || eventType == "emote" )
                                     { Text.AlignRight }
                color: if( eventType == "other" ) { "darkgrey" }
                       else if( eventType == "emote" ) { "darkblue" }
                       else { "black" }

            }
            Rectangle {
                color: highlight ? "orange" : "white"
                Layout.fillWidth: true
                Layout.minimumHeight: childrenRect.height

                Column {
                    spacing: 0
                    width: parent.width

                    TextEdit {
                        id: contentField
                        selectByMouse: true; readOnly: true; font: timelabel.font;
                        text: content
                        height: eventType == "image" ? 0 : implicitHeight
                        wrapMode: Text.Wrap; width: parent.width
                        color: if( eventType == "other" ) { "darkgrey" }
                               else if( eventType == "emote" ) { "darkblue" }
                               else { "black" }
                    }
                    Loader {
                        asynchronous: true
                        visible: status == Loader.Ready
                        width: parent.width
                        property string sourceText: toolTip

                        sourceComponent: showSource.checked ? sourceArea : undefined
                    }
                    Image {
                        id: imageField
                        fillMode: Image.PreserveAspectFit
                        width: eventType == "image" ? parent.width : 0

                        sourceSize: eventType == "image" ? "500x500" : "0x0"
                        source: eventType == "image" ? content : ""
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
    }

    Component {
        id: sourceArea

        TextArea {
            selectByMouse: true; readOnly: true; font.family: "Monospace"
            text: sourceText
        }
    }
}
