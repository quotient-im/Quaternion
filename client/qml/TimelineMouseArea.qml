import QtQuick 2.2

MouseArea {
    onWheel: chatView.onWheel(wheel)
    onReleased: controller.focusInput()
}
