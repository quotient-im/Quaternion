import QtQuick 2.3
import QtQuick.Controls 1.4

Rectangle {
    anchors.fill: parent

    ListView {
        id: chatView
        anchors.fill: parent
        //width: 200; height: 250

        model: messageModel
        delegate: messageDelegate
        flickableDirection: Flickable.VerticalFlick
    }

    Component {
        id: messageDelegate

        Row {
            id: message
            width: parent.width

            Label { text: time.toLocaleString(Qt.locale("de_DE"), "'<'hh:mm:ss'>'"); color: "grey" }
            Label { text: author; width: 120; elide: Text.ElideRight; }
            Label { text: content; wrapMode: Text.Wrap; width: parent.width - (x - parent.x) - spacing
                    ToolTipArea { tip { text: toolTip; color: "#999999"; zParent: message } }
            }
            spacing: 3
        }
    }
}