import QtQuick 2.6
import QtQuick.Controls 2.15

Button {
    id: tButton
    contentItem: Text {
        text: tButton.text
        font: settings.font
        fontSizeMode: Text.VerticalFit
        minimumPointSize: settings.font.pointSize - 3
        color: foreground
        renderType: settings.render_type
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
    }
}
