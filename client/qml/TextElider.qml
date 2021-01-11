import QtQuick 2.4

TextMetrics {
    property var textControl

    font: textControl.font
    elide: Text.ElideRight
    elideWidth: textControl.width
}
