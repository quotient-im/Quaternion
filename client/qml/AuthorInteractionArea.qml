TimelineMouseArea {
    required property var authorId

    enabled: parent.visible
    anchors.fill: parent
    cursorShape: Qt.PointingHandCursor
    acceptedButtons: Qt.LeftButton|Qt.MiddleButton
    hoverEnabled: true
    onEntered: controller.showStatusMessage(authorId)
    onExited: controller.showStatusMessage("")
    onClicked: (mouse) => {
        controller.resourceRequested(authorId,
                                     mouse.button === Qt.LeftButton
                                     ? "mention" : "_interactive")
    }
}
