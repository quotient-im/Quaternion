import QtQuick 2.0

Rectangle {
    id:tooltip
    property alias text: textContainer.text
    property int verticalPadding: 1
    property int horizontalPadding: 5
    property Item zParent: parent
    width: textContainer.width + horizontalPadding * 2
    height: textContainer.height + verticalPadding * 2
    color: "#aa999999"
    Text {
        anchors.centerIn: parent
        id:textContainer
        text: "Gering geding ding ding!"
    }
    NumberAnimation {
        id: fadein
        target: tooltip
        property: "opacity"
        easing.type: Easing.InOutQuad
        duration: 300
        from: 0
        to: 1
    }
    NumberAnimation {
        id: fadeout
        target: tooltip
        property: "opacity"
        easing.type: Easing.InOutQuad
        from: 1
        to: 0
        onStopped: visible = false;
    }
    visible:false
    onVisibleChanged: if(visible)fadein.start();
    function show(){
        visible = true;
        zParent.z = 2;
        fadein.start();
    }
    function hide(){
        this.parent.parent.parent.z = 0;
        fadeout.start();
    }
}
