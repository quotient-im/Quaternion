import QtQuick 2.2

/*
 * Unfortunately, TextEdit captures LeftButton events for text selection in a way which
 * is not compatible with our focus-cancelling mechanism, so we took over the task here.
 */
MouseArea {
    property var textEdit: parent
    property int selectionMode: TextEdit.SelectCharacters

    anchors.fill: parent
    acceptedButtons: Qt.LeftButton
    preventStealing: true

    onPressed: (mouse) => {
        var x = mouse.x
        var y = mouse.y
        if (textEdit.flickableItem) {
            x += textEdit.flickableItem.contentX
            y += textEdit.flickableItem.contentY
        }
        var hasSelection = textEdit.selectionEnd > textEdit.selectionStart
        if (hasSelection && controller.getModifierKeys() & Qt.ShiftModifier) {
            textEdit.moveCursorSelection(textEdit.positionAt(x, y), selectionMode)
        } else {
            textEdit.cursorPosition = textEdit.positionAt(x, y)
            if (chatView.textEditWithSelection)
                chatView.textEditWithSelection.deselect()
        }
    }
    onClicked: {
        if (textEdit.hoveredLink)
            textEdit.linkActivated(textEdit.hoveredLink)
    }
    onDoubleClicked: {
        selectionMode = TextEdit.SelectWords
        textEdit.selectWord()
    }
    onReleased: {
        selectionMode = TextEdit.SelectCharacters
        controller.setGlobalSelectionBuffer(textEdit.selectedText)
        chatView.textEditWithSelection = textEdit

        controller.focusInput()
    }
    onPositionChanged: (mouse) => {
        var x = mouse.x
        var y = mouse.y
        if (textEdit.flickableItem) {
            x += textEdit.flickableItem.contentX
            y += textEdit.flickableItem.contentY
        }
        textEdit.moveCursorSelection(textEdit.positionAt(x, y), selectionMode)
    }
}
