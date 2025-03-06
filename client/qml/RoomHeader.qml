import QtQuick 2.15
import QtQuick.Controls 2.3
import Quotient 1.0

Frame {
    id: roomHeader

    required property Room room
    property bool showTopic: true

    height: headerText.height + 11
    padding: 3
    visible: !!room
    
    Avatar {
        id: roomAvatar
        anchors.verticalCenter: headerText.verticalCenter
        anchors.left: parent.left
        anchors.margins: 2
        height: headerText.height
        // implicitWidth on its own doesn't respect the scale down of
        // the received image (that almost always happens)
        width: Math.min(implicitHeight > 0 ? headerText.height / implicitHeight * implicitWidth
                                           : 0,
                        parent.width / 2.618) // Golden ratio - just for fun
        
        // Safe upper limit (see also topicField)
        sourceSize: Qt.size(-1, settings.lineSpacing * 9)
        
        AnimationBehavior on width {
            NormalNumberAnimation { easing.type: Easing.OutQuad }
        }
    }
    
    Column {
        id: headerText
        anchors.left: roomAvatar.right
        anchors.right: versionActionButton.left
        anchors.top: parent.top
        anchors.margins: 2
        
        spacing: 2
        
        readonly property int innerLeftPadding: 4
        
        TextArea {
            id: roomName
            width: roomNameMetrics.advanceWidth + leftPadding
            height: roomNameMetrics.height
            clip: true
            padding: 0
            leftPadding: headerText.innerLeftPadding
            
            TextMetrics {
                id: roomNameMetrics
                font: roomName.font
                elide: Text.ElideRight
                elideWidth: headerText.width
                text: roomHeader.room?.displayName ?? ""
            }
            
            text: roomNameMetrics.elidedText
            placeholderText: qsTr("(no name)")
            
            font.bold: true
            renderType: settings.render_type
            readOnly: true
            
            hoverEnabled: text !== "" &&
                          (roomNameMetrics.text != roomNameMetrics.elidedText
                           || roomName.lineCount > 1)
            ToolTip.visible: hovered
            ToolTip.text: roomHeader.room?.displayNameForHtml ?? ""
        }
        
        Label {
            id: versionNotice

            property alias room: roomHeader.room

            visible: room?.isUnstable || room?.successorId !== ""
            width: parent.width
            leftPadding: headerText.innerLeftPadding
            
            text: room?.successorId !== "" ? qsTr("This room has been upgraded.")
                                           : room?.isUnstable ? qsTr("Unstable room version!") : ""
            elide: Text.ElideRight
            font.italic: true
            renderType: settings.render_type
            
            HoverHandler {
                id: versionHoverHandler
                enabled: parent.truncated
            }
            ToolTip.text: text
            ToolTip.visible: versionHoverHandler.hovered
        }
        
        ScrollView {
            id: topicField
            visible: roomHeader.showTopic
            width: parent.width
            // Allow 5 full (actually, 6 minus padding) lines of the topic
            // but not more than 20% of the timeline vertical space
            height:
                Math.min(topicText.implicitHeight, parent.height / 5, settings.lineSpacing * 6)
            
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            
            AnimationBehavior on height {
                NormalNumberAnimation { easing.type: Easing.OutQuad }
            }
            
            // FIXME: The below TextArea+MouseArea is a massive copy-paste
            // from textFieldImpl and its respective MouseArea in
            // TimelineItem.qml. Maybe make a separate component for these
            // (RichTextField?).
            TextArea {
                id: topicText
                padding: 2
                leftPadding: headerText.innerLeftPadding
                rightPadding: topicField.ScrollBar.vertical.visible
                              ? topicField.ScrollBar.vertical.width : padding
                
                text: roomHeader.room?.prettyPrint(roomHeader.room?.topic) ?? ""
                placeholderText: qsTr("(no topic)")
                textFormat: TextEdit.RichText
                renderType: settings.render_type
                readOnly: true
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                hoverEnabled: true
                
                onLinkActivated:
                    (link) => controller.resourceRequested(link)
            }
        }
    }
    MouseArea {
        anchors.fill: headerText
        acceptedButtons: Qt.MiddleButton | Qt.RightButton
        cursorShape: topicText.hoveredLink ? Qt.PointingHandCursor : Qt.IBeamCursor
        
        onClicked: (mouse) => {
                       if (topicText.hoveredLink)
                       controller.resourceRequested(topicText.hoveredLink,
                                                    "_interactive")
                       else if (mouse.button === Qt.RightButton)
                       headerContextMenu.popup()
                   }
        Menu {
            id: headerContextMenu
            MenuItem {
                text: roomHeader.showTopic ? qsTr("Hide topic") : qsTr("Show topic")
                onTriggered: roomHeader.showTopic = !roomHeader.showTopic
            }
        }
    }
    Button {
        id: versionActionButton

        property alias room: roomHeader.room

        visible: (room?.isUnstable && room?.canSwitchVersions()) || room?.successorId !== ""
        anchors.verticalCenter: headerText.verticalCenter
        anchors.right: parent.right
        width: visible * implicitWidth
        text: room?.successorId !== "" ? qsTr("Go to\nnew room") : qsTr("Room\nsettings")
        
        onClicked:
            if (room.successorId !== "")
                controller.resourceRequested(room.successorId, "join")
            else
                controller.roomSettingsRequested()
    }
}
