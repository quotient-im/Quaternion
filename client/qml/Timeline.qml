// SPDX-FileCopyrightText: 2020 Carl Schwan <carlschwan@kde.org>
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.2 as QQC2
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.0
import QtQuick.Layouts 1.1
import Quotient 1.0

QQC2.Pane {
    id: root

    TimelineSettings {
        id: settings
        readonly property bool use_shuttle_dial: value("UI/use_shuttle_dial", true)

        Component.onCompleted: console.log("Using timeline font: " + font)
    }

    SystemPalette { id: defaultPalette; colorGroup: SystemPalette.Active }
    SystemPalette { id: disabledPalette; colorGroup: SystemPalette.Disabled }

    function humanSize(bytes)
    {
        if (!bytes)
            return qsTr("Unknown", "Unknown attachment size")
        if (bytes < 4000)
            return qsTr("%Ln byte(s)", "", bytes)
        bytes = Math.round(bytes / 100) / 10
        if (bytes < 2000)
            return qsTr("%L1 KB").arg(bytes)
        bytes = Math.round(bytes / 100) / 10
        if (bytes < 2000)
            return qsTr("%L1 MB").arg(bytes)
        return qsTr("%L1 GB").arg(Math.round(bytes / 100) / 10)
    }

    function mixColors(baseColor, mixedColor, mixRatio)
    {
        return Qt.tint(baseColor,
                Qt.rgba(mixedColor.r, mixedColor.g, mixedColor.b, mixRatio))
    }

    background: Rectangle {
        color: palette.base
    }

    padding: 0

    contentItem: ColumnLayout {
        QQC2.Control {
            id: roomHeader

            Layout.fillWidth: true
            Layout.maximumHeight: 65

            background: Rectangle {
                color: palette.alternateBase
            }

            visible: room

            padding: 0
            topPadding: 0
            bottomPadding: 0
            leftPadding: 0
            rightPadding: 0

            contentItem: RowLayout {
                Image {
                    Layout.maximumWidth: 65
                    Layout.alignment: Qt.AlignTop
                    Layout.maximumHeight: 65
                    fillMode: Image.PreserveAspectFit
                    source: room && room.avatarMediaId
                            ? "image://mtx/" + room.avatarMediaId : ""

                    AnimationBehavior on width {
                        NormalNumberAnimation { easing.type: Easing.OutQuad }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    TextEdit {
                        id: roomName
                        Layout.fillWidth: true

                        readonly property bool hasName: room && room.displayName !== ""
                        text: hasName ? room.displayName : qsTr("(no name)")
                        color: (hasName ? defaultPalette : disabledPalette).windowText
                        ToolTipArea { text: parent.hasName ? room.htmlSafeName : "" }

                        font.bold: true
                        font.family: settings.font.family
                        font.pointSize: settings.font.pointSize
                        renderType: settings.render_type
                        readOnly: true
                        selectByKeyboard: true
                        selectByMouse: true
                    }
                    Label {
                        id: versionNotice
                        visible: room && (room.isUnstable || room.successorId !== "")
                        Layout.fillWidth: true

                        text: !room ? "" :
                            room.successorId !== ""
                                      ? qsTr("This room has been upgraded.") :
                            room.isUnstable ? qsTr("Unstable room version!") : ""
                        font.italic: true
                        font.family: settings.font.family
                        font.pointSize: settings.font.pointSize
                        renderType: settings.render_type
                        ToolTipArea { text: parent.text }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        QQC2.Label {
                            readonly property bool hasTopic: room && room.topic.length > 0

                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            id: topicText

                            text: hasTopic ? room.prettyPrint(room.topic) : qsTr("(no topic)")
                            color: (hasTopic ? defaultPalette : disabledPalette).windowText
                            font: settings.font * 0.9
                            renderType: settings.render_type
                            maximumLineCount: 2
                            wrapMode: Text.Wrap
                            elide: Text.ElideRight

                            onHoveredLinkChanged: controller.showStatusMessage(hoveredLink)

                            onLinkActivated: controller.resourceRequested(link)
                            QQC2.ToolTip.text: text
                            QQC2.ToolTip.visible: moreButton.checked
                        }
                        QQC2.ToolButton {
                            id: moreButton
                            text: qsTr("More...")
                            checkable: true
                            visible: topicText.truncated
                        }
                    }

                    HoverHandler {
                        cursorShape: topicText.hoveredLink
                                     ? Qt.PointingHandCursor : Qt.IBeamCursor

                    }

                    TapHandler {
                        acceptedButtons: Qt.MiddleButton
                        onTapped: {
                            if (topicText.hoveredLink) {
                                controller.resourceRequested(topicText.hoveredLink,
                                    "_interactive");
                            }
                        }
                    }
                }
                QQC2.ToolButton {
                    id: versionActionButton
                    visible: room && ((room.isUnstable && room.canSwitchVersions())
                                      || room.successorId !== "")
                    width: visible * implicitWidth
                    text: !room ? "" : room.successorId !== ""
                                        ? qsTr("Go to\nnew room") : qsTr("Room\nsettings")

                    onClicked: {
                        if (room.successorId !== "") {
                            controller.resourceRequested(room.successorId, "join");
                        } else {
                            controller.roomSettingsRequested();
                        }
                    }
                }
            }
        }

        QQC2.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            contentItem: ListView {
                id: chatView

                DropArea {
                    anchors.fill: parent
                    onEntered: if (!room) { drag.accepted = false }
                    onDropped: {
                        if (drop.hasUrls) {
                            controller.fileDrop(drop.urls)
                            drop.acceptProposedAction()
                        } else if (drop.hasText) {
                            controller.textDrop(drop.text)
                            drop.acceptProposedAction()
                        }
                    }
                }

                Rectangle {
                    anchors.right: parent.left
                    anchors.top: parent.top
                    width: childrenRect.width + 3
                    height: childrenRect.height + 3
                    color: defaultPalette.alternateBase
                    opacity: chatView.bottommostVisibleIndex >= 0
                        && scrollTimer.running ? 0.8 : 0
                    AnimationBehavior on opacity { FastNumberAnimation { } }

                    QQC2.Label {
                        font.bold: true
                        font.family: settings.font.family
                        font.pointSize: settings.font.pointSize
                        opacity: 0.8
                        renderType: settings.render_type
                        text: (chatView.bottommostVisibleIndex === 0
                               ? qsTr("Latest events") : qsTr("%Ln events back from now","",
                                                              chatView.bottommostVisibleIndex))
                              + '\n' + qsTr("%Ln cached", "", chatView.count)
                        horizontalAlignment: Label.AlignRight
                    }
                }

                Rectangle {
                    id: scrollindicator
                    opacity: chatView.atYEnd ? 0 : 0.5
                    color: defaultPalette.text
                    height: 30
                    radius: height/2
                    width: height
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: width/2
                    anchors.bottomMargin: chatView.atYEnd ? -height : height/2
                    AnimationBehavior on opacity {
                        NormalNumberAnimation { easing.type: Easing.OutQuad }
                    }
                    AnimationBehavior on anchors.bottomMargin {
                        NormalNumberAnimation { easing.type: Easing.OutQuad }
                    }
                    Image {
                        anchors.fill: parent
                        source: "qrc:///scrolldown.svg"
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            chatView.positionViewAtBeginning()
                            chatView.saveViewport()
                        }
                        cursorShape: Qt.PointingHandCursor
                    }
                }

                // This covers the area above a short chatView.
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.AllButtons
                    onReleased: controller.focusInput()
                }

                model: messageModel
                delegate: TimelineItem {
                    width: chatView.width
                    view: chatView
                    moving: chatView.moving
                }
                verticalLayoutDirection: ListView.BottomToTop
                boundsBehavior: Flickable.StopAtBounds
    //            pixelAligned: true // Causes false-negatives in atYEnd
                cacheBuffer: 200

                section.property: "section"

                readonly property bool atBeginning: contentY + height == 0
                readonly property int bottommostVisibleIndex: count > 0 ?
                    atBeginning ? 0 : indexAt(contentX, contentY + height - 1) : -1
                readonly property bool noNeedMoreContent:
                    !room || room.eventsHistoryJob || room.allHistoryLoaded

                /// The number of events per height unit - always positive
                readonly property real eventDensity:
                    contentHeight > 0 && count > 0 ? count / contentHeight : 0.03
                    // 0.03 is just an arbitrary reasonable number

                property int lastRequestedEvents: 0
                property var textEditWithSelection

                function ensurePreviousContent() {
                    if (noNeedMoreContent)
                        return

                    // Take the current speed, or assume we can scroll 8 screens/s
                    var velocity = moving ? -verticalVelocity : height * 8
                    // Check if we're about to bump into the ceiling in
                    // 2 seconds and if yes, request the amount of messages
                    // enough to scroll at this rate for 3 more seconds
                    if (velocity > 0 && contentY - velocity*2 < originY) {
                        lastRequestedEvents = velocity * eventDensity * 3
                        room.getPreviousContent(lastRequestedEvents)
                    }
                }
                onContentYChanged: ensurePreviousContent()
                onContentHeightChanged: ensurePreviousContent()

                function saveViewport() {
                    room.saveViewport(indexAt(contentX, contentY),
                                      bottommostVisibleIndex)
                }

                function onModelReset() {
                    console.log("Model timeline reset")
                    if (room)
                    {
                        var lastScrollPosition = room.savedTopVisibleIndex()
                        if (lastScrollPosition === 0)
                            positionViewAtBeginning()
                        else
                        {
                            console.log("Scrolling to position", lastScrollPosition)
                            positionViewAtIndex(lastScrollPosition, ListView.Contain)
                        }
                    }
                }

                function scrollUp(dy) {
                    contentY = Math.max(originY, contentY - dy)
                }
                function scrollDown(dy) {
                    contentY = Math.min(originY + contentHeight - height,
                                        contentY + dy)
                }

                function onWheel(wheel) {
                    if (wheel.angleDelta.x === 0) {
                        var yDelta = wheel.angleDelta.y * 10 / 36

                        if (yDelta > 0)
                            scrollUp(yDelta)
                        else
                            scrollDown(-yDelta)
                        wheel.accepted = true
                    } else {
                        wheel.accepted = false
                    }
                }
                Connections {
                    target: controller
                    onPageUpPressed:
                        chatView.scrollUp(chatView.height
                                          - sectionBanner.childrenRect.height)

                    onPageDownPressed:
                        chatView.scrollDown(chatView.height
                                            - sectionBanner.childrenRect.height)
                    onScrollViewTo: chatView.positionViewAtIndex(currentIndex, ListView.Contain)
                }

                Component.onCompleted: {
                    console.log("QML view loaded")
                    // FIXME: This is not on the right place: ListView may or
                    // may not have updated his structures according to the new
                    // model by now
                    model.modelReset.connect(onModelReset)
                }

                onMovementEnded: saveViewport()

                populate: AnimatedTransition {
                    FastNumberAnimation { property: "opacity"; from: 0; to: 1 }
                }

                add: AnimatedTransition {
                    FastNumberAnimation { property: "opacity"; from: 0; to: 1 }
                }

                move: AnimatedTransition {
                    FastNumberAnimation { property: "y"; }
                    FastNumberAnimation { property: "opacity"; to: 1 }
                }

                displaced: AnimatedTransition {
                    FastNumberAnimation {
                        property: "y";
                        easing.type: Easing.OutQuad
                    }
                    FastNumberAnimation { property: "opacity"; to: 1 }
                }

                Timer {
                    id: scrollTimer
                    interval: 500
                    onTriggered: chatView.saveViewport();
                }

                onContentXChanged: scrollTimer.restart();

                // itemAt is a function rather than a property, so it doesn't
                // produce a QML binding; the piece with contentHeight compensates.
                readonly property var underlayingItem: contentHeight >= height &&
                    itemAt(contentX, contentY + sectionBanner.height - 2)
                readonly property bool sectionBannerVisible: underlayingItem &&
                    (!underlayingItem.sectionVisible || underlayingItem.y < contentY)

                Rectangle {
                    id: sectionBanner
                    z: 3 // On top of ListView sections that have z=2
                    anchors.left: parent.left
                    anchors.top: parent.top
                    width: childrenRect.width + 2
                    height: childrenRect.height + 2
                    visible: chatView.sectionBannerVisible
                    color: defaultPalette.window
                    opacity: 0.8
                    QQC2.Label {
                        font.bold: true
                        font.family: settings.font.family
                        font.pointSize: settings.font.pointSize
                        opacity: 0.8
                        renderType: settings.render_type
                        text: chatView.underlayingItem ?
                                  chatView.underlayingItem.ListView.section : ""
                    }
                }
            }
        }
    }
}
