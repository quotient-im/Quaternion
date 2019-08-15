import QtQuick 2.6
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4

ToolButton {
    width: visible * implicitWidth
    height: visible * textField.height
    anchors.top: textField.top
    anchors.rightMargin: 2

    style: ButtonStyle {
        label: Text {
          text: control.text
          font.family: settings.font_family
          font.pointSize: settings.font_pointSize - 2
        }
      }
}
