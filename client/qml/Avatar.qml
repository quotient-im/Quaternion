import QtQuick 2.15

Image {
    id: avatar
    readonly property var forRoom: root.room
    /* readonly */ property var forMember

    property string sourceId: forMember?.avatarUrl ?? forRoom?.avatarUrl ?? ""
    source: sourceId
    cache: false // Quotient::Avatar takes care of caching
    fillMode: Image.PreserveAspectFit

    function reload() {
        source = ""
        source = Qt.binding(function() { return sourceId })
    }

    Connections {
        target: forRoom
        function onAvatarChanged() { avatar.reload() }
        function onMemberAvatarUpdated(member) {
            if (avatar.forMember && member?.id === avatar.forMember.id)
                avatar.reload()
        }
    }
}
