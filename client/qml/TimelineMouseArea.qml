import QtQuick 2.2

MouseArea {
    onWheel: (wheel) => { chatView.onWheel(wheel) }
    onReleased: controller.focusInput()
}
