import QtQuick 2.15

Image {
    readonly property var forRoom: root.room
    /* readonly */ property var forMember

    property string sourceId: forRoom ? "image://thumbnail/" + forRoom.id
                                     + (forMember ? '/' + forMember.id : "")
                                   : ""
    source: sourceId
    cache: false // Quotient::Avatar takes care of caching
    fillMode: Image.PreserveAspectFit

    function reload() {
        source = ""
        source = Qt.binding(function() { return sourceId })
    }

    Connections {
        target: forRoom
        function onAvatarChanged() { parent.reload() }
        function onMemberAvatarChanged(member) {
            if (member === parent.forMember)
                parent.reload()
        }
    }
}
