// from https://github.com/bobbaluba/qmltooltip

import QtQuick 2.0

MouseArea {
    property alias tip: tip
    property alias text: tip.text
    property alias hideDelay: hideTimer.interval
    property alias showDelay: showTimer.interval
    property bool enabled: true
    id: mouseArea
    z: 21
    acceptedButtons: Qt.NoButton
    anchors.fill: parent
    hoverEnabled: true
    Timer {
        id:showTimer
        interval: 1000
        running: mouseArea.containsMouse && !tip.visible && mouseArea.enabled
        onTriggered: tip.show();
    }
    Timer {
        id:hideTimer
        interval: 100
        running: !mouseArea.containsMouse && tip.visible
        onTriggered: tip.hide();
    }
    ToolTip{
        id:tip
    }
}

