import QtQuick 2.0
import QtQuick.Controls 1.0

Rectangle {
    id: root

    signal getPreviousContent()

    function scrollToBottom() {
        chatView.positionViewAtEnd();
    }

    ScrollView {
    anchors.fill: parent

        ListView {
            id: chatView
            anchors.fill: parent
            //width: 200; height: 250

            model: messageModel
            delegate: messageDelegate
            flickableDirection: Flickable.VerticalFlick
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
    }

    Component {
        id: messageDelegate

        Row {
            id: message
            width: parent.width

            Label {
                id: timelabel
                text: time.toLocaleString(Qt.locale("de_DE"), "'<'hh:mm:ss'>'")
                color: "grey"
            }
            Label {
                width: 120; elide: Text.ElideRight;
                text: eventType == "message" || eventType == "image" ? author : "***"
                horizontalAlignment: if( eventType == "other" || eventType == "emote" )
                                     { Text.AlignRight }
                color: if( eventType == "other" ) { "darkgrey" }
                       else if( eventType == "emote" ) { "darkblue" }
                       else { "black" }

            }
            Rectangle {
                color: highlight ? "orange" : "white"
                height: contentField.height + imageField.height
                width: parent.width - (x - parent.x) - spacing
                TextEdit {
                        id: contentField
                        selectByMouse: true; readOnly: true; font: timelabel.font;
                        text: content
                        wrapMode: Text.Wrap; width: parent.width
                        color: if( eventType == "other" ) { "darkgrey" }
                               else if( eventType == "emote" ) { "darkblue" }
                               else { "black" }
                        ToolTipArea {
                            tip { text: toolTip; color: "#999999"; zParent: message }
                            enabled: debug
                        }
                }
                Image {
                    id: imageField
                    anchors.top: contentField.bottom
                    sourceSize.width: eventType == "image" ? 500 : 0
                    sourceSize.height: eventType == "image" ? 500 : 0
                    source: eventType == "image" ? content : ""
                }
            }
            spacing: 3
        }
    }
}
