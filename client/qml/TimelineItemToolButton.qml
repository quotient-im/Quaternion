import QtQuick 2.6
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4

ToolButton {
    width: visible * implicitWidth
    height: visible * parent.height
    anchors.top: parent.top
    anchors.rightMargin: 2

    style: ButtonStyle {
        label: Text {
            text: control.text
            font.family: settings.font.family
            font.pointSize: settings.font.pointSize - 2
            renderType: settings.render_type
        }
      }
}
