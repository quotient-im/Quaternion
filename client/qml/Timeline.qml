import QtQuick 2.4
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.1
import QtGraphicalEffects 1.0 // For fancy highlighting
import Quotient 1.0

Rectangle {
    id: root

    TimelineSettings {
        id: settings
        readonly property bool use_shuttle_dial: value("UI/use_shuttle_dial", true)

        Component.onCompleted: console.log("Using timeline font: " + font)
    }
    SystemPalette { id: defaultPalette; colorGroup: SystemPalette.Active }
    SystemPalette { id: disabledPalette; colorGroup: SystemPalette.Disabled }

    color: defaultPalette.base
    radius: 2

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

    Rectangle {
        id: roomHeader

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: headerText.height + 5

        color: defaultPalette.window
        border.color: disabledPalette.windowText
        radius: 2
        visible: room

        Image {
            id: roomAvatar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: 2
            height: headerText.height

            source: room && room.avatarMediaId
                    ? "image://mtx/" + room.avatarMediaId : ""
            sourceSize { // Kinda safe upper limits
                height: root.height / 3
                width: parent.width
            }
            fillMode: Image.PreserveAspectFit

            AnimationBehavior on width {
                NormalNumberAnimation { easing.type: Easing.OutQuad }
            }
        }
        Binding {
            target: roomAvatar
            property: "width"
            value: roomHeader.width ? roomHeader.width / 2 : 0
            when: roomAvatar.width && roomHeader.width
                  && roomAvatar.width > roomHeader.width / 2
        }

        Column {
            id: headerText
            anchors.left: roomAvatar.right
            anchors.right: versionActionButton.left
            anchors.top: parent.top
            anchors.margins: 2

            spacing: 2

            TextEdit {
                id: roomName
                width: roomNameMetrics.advanceWidth
                height: settings.defaultText.height
                clip: true

                readonly property bool hasName: room && room.displayName !== ""
                TextMetrics {
                    id: roomNameMetrics
                    font: roomName.font
                    elide: Text.ElideRight
                    elideWidth: headerText.width
                    text: roomName.hasName ? room.displayName : qsTr("(no name)")
                }

                text: roomNameMetrics.elidedText
                color: (hasName ? defaultPalette : disabledPalette).windowText

                font.bold: true
                font.family: settings.font.family
                font.pointSize: settings.font.pointSize
                renderType: settings.render_type
                readOnly: true
                selectByKeyboard: true
                selectByMouse: true

                ToolTipArea {
                    enabled: roomName.hasName &&
                             (roomNameMetrics.text != roomNameMetrics.elidedText
                             || roomName.lineCount > 1)
                    text: room ? room.htmlSafeDisplayName : ""
                }
            }

            Label {
                id: versionNotice
                visible: room && (room.isUnstable || room.successorId !== "")
                width: parent.width

                text: !room ? "" :
                    room.successorId !== ""
                              ? qsTr("This room has been upgraded.") :
                    room.isUnstable ? qsTr("Unstable room version!") : ""
                elide: Text.ElideRight
                font.italic: true
                font.family: settings.font.family
                font.pointSize: settings.font.pointSize
                renderType: settings.render_type
                ToolTipArea {
                    enabled: parent.truncated
                    text: parent.text
                }
            }
            ScrollView {
                id: topicField
                width: parent.width
                height: Math.min(topicText.contentHeight,
                                 room ? root.height / 5 : 0)

                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                AnimationBehavior on height {
                    NormalNumberAnimation { easing.type: Easing.OutQuad }
                }

                // FIXME: The below TextEdit+MouseArea is a massive copy-paste
                // from TimelineItem.qml. We need to make a separate component
                // for these (RichTextField?).
                TextEdit {
                    id: topicText
                    width: topicField.width

                    readonly property bool hasTopic: room && room.topic !== ""
                    text: hasTopic
                          ? room.prettyPrint(room.topic) : qsTr("(no topic)")
                    color:
                        (hasTopic ? defaultPalette : disabledPalette).windowText
                    textFormat: TextEdit.RichText
                    font: settings.font
                    renderType: settings.render_type
                    readOnly: true
                    selectByKeyboard: true;
                    selectByMouse: true;
                    wrapMode: TextEdit.Wrap

                    onHoveredLinkChanged:
                        controller.showStatusMessage(hoveredLink)

                    onLinkActivated: controller.resourceRequested(link)
                }
            }
        }
        MouseArea {
            anchors.fill: headerText
            acceptedButtons: Qt.MiddleButton
            cursorShape: topicText.hoveredLink
                         ? Qt.PointingHandCursor : Qt.IBeamCursor

            onClicked: {
                if (topicText.hoveredLink)
                    controller.resourceRequested(topicText.hoveredLink,
                                                 "_interactive")
            }
        }
        Button {
            id: versionActionButton
            visible: room && ((room.isUnstable && room.canSwitchVersions())
                              || room.successorId !== "")
            anchors.verticalCenter: headerText.verticalCenter
            anchors.right: parent.right
            width: visible * implicitWidth
            text: !room ? "" : room.successorId !== ""
                                ? qsTr("Go to\nnew room") : qsTr("Room\nsettings")

            onClicked:
                if (room.successorId !== "")
                    controller.resourceRequested(room.successorId, "join")
                else
                    controller.roomSettingsRequested()
        }
    }

    DropArea {
        anchors.fill: parent
        onEntered: if (!room) drag.accepted = false
        onDropped: {
            if (drop.hasUrls) {
                controller.fileDrop(drop.urls)
                drop.acceptProposedAction()
            } else if (drop.hasHtml) {
                controller.htmlDrop(drop.html)
                drop.acceptProposedAction()
            } else if (drop.hasText) {
                controller.textDrop(drop.text)
                drop.acceptProposedAction()
            }
        }
    }
    ScrollView {
        id: chatScrollView
        anchors.top: roomHeader.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy:
            settings.use_shuttle_dial ? ScrollBar.AlwaysOff
                                      : ScrollBar.AlwaysOn
        ScrollBar.vertical.interactive: true
        ScrollBar.vertical.active: true

        ListView {
            id: chatView

            model: messageModel
            delegate: TimelineItem {
                width: chatView.width - scrollerArea.width
                view: chatView
                moving: chatView.moving || shuttleDial.value
            }
            verticalLayoutDirection: ListView.BottomToTop
            flickableDirection: Flickable.VerticalFlick
            flickDeceleration: 8000
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
            property real readMarkerContentPos: originY
            readonly property real readMarkerViewportPos:
                readMarkerContentPos < contentY ? 0 :
                readMarkerContentPos > contentY + height ? height + readMarkerLine.height :
                readMarkerContentPos - contentY

            function parkReadMarker() {
                readMarkerContentPos = Qt.binding(function() {
                    return messageModel.readMarkerVisualIndex > indexAt(contentX, contentY)
                           ? originY : contentY + contentHeight
                })
                console.log("Read marker parked at index",
                            messageModel.readMarkerVisualIndex
                            + ", content pos", chatView.readMarkerContentPos,
                            "(full range is", chatView.originY, "-",
                            chatView.originY + chatView.contentHeight,
                            "as of now)")
            }

            function ensurePreviousContent() {
                if (noNeedMoreContent)
                    return

                // Take the current speed, or assume we can scroll 8 screens/s
                var velocity = moving ? -verticalVelocity
                                      : chatScrollView.height * 8
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
                    forceLayout()
                    // Load events if there are not enough of them
                    ensurePreviousContent()
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
                if (contentHeight > height)
                    contentY = Math.max(originY, contentY - dy)
            }
            function scrollDown(dy) {
                if (contentHeight > height)
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
                model.modelAboutToBeReset.connect(parkReadMarker)
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

            Behavior on contentY {
                enabled: !chatView.moving && settings.enable_animations
                SmoothedAnimation {
                    id: scrollAnimation
                    // It would mislead the benchmark below
                    duration: settings.animations_duration_ms_impl > 0 ? settings.fast_animations_duration_ms / 4 : 0
                    maximumEasingTime: settings.animations_duration_ms_impl > 0 ? settings.fast_animations_duration_ms / 2 : 0

                    onRunningChanged: {
                        if (!running) {
                            chatView.saveViewport()
                            if (settings.animations_duration_ms_impl == 0)
                                console.timeEnd("scroll")
                        } else {
                            if (settings.animations_duration_ms_impl == 0)
                                console.time("scroll")
                        }
                    }
            }}

            AnimationBehavior on readMarkerContentPos {
                NormalNumberAnimation { easing.type: Easing.OutQuad }
            }

            // This covers the area above the items if there are not enough
            // of them to fill the viewport
            MouseArea {
                z: -1
                anchors.fill: parent
                acceptedButtons: Qt.AllButtons
                onReleased: controller.focusInput()
            }

            Rectangle {
                id: readShade

                visible: chatView.count > 0
                anchors.top: parent.top
                anchors.topMargin: chatView.originY > chatView.contentY
                                   ? chatView.originY - chatView.contentY : 0
                /// At the bottom of the read shade is the read marker. If
                /// the last read item is on the screen, the read marker is at
                /// the item's bottom; otherwise, it's just beyond the edge of
                /// chatView in the direction of the read marker index (or the
                /// timeline, if the timeline is short enough).
                /// @sa readMarkerViewportPos
                height: chatView.readMarkerViewportPos - anchors.topMargin
                anchors.left: parent.left
                width: parent.width
                z: -1
                opacity: 0.2

                radius: readMarkerLine.height
                color: mixColors(disabledPalette.base, defaultPalette.highlight, 0.5)
            }
            Rectangle {
                id: readMarkerLine

                visible: chatView.count > 0
                width: parent.width
                anchors.bottom: readShade.bottom
                height: 4
                z: 2.5 // On top of any ListView content, below the banner

                gradient: Gradient {
                    GradientStop { position: 0; color: "transparent" }
                    GradientStop { position: 1; color: defaultPalette.highlight }
                }
            }


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
                Label {
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

    // === Timeline map ===
    // Only used with the shuttle scroller for now

    Rectangle {
        id: cachedEventsBar

        property int requestedHistoryEventsCount:
            room && room.eventsHistoryJob
            ? chatView.lastRequestedEvents : 0

        AnimationBehavior on requestedHistoryEventsCount {
            NormalNumberAnimation { }
        }

        property real averageEvtHeight:
            chatView.count + requestedHistoryEventsCount > 0
            ? chatView.height
              / (chatView.count + requestedHistoryEventsCount)
            : 0

        AnimationBehavior on averageEvtHeight {
            FastNumberAnimation { }
        }

        anchors.horizontalCenter: shuttleDial.horizontalCenter
        anchors.bottom: chatScrollView.bottom
        anchors.bottomMargin:
            averageEvtHeight * chatView.bottommostVisibleIndex
        width: shuttleDial.width
        height: chatView.bottommostVisibleIndex < 0 ? 0 :
            averageEvtHeight
            * (chatView.count - chatView.bottommostVisibleIndex)
        visible: shuttleDial.visible

        color: defaultPalette.highlight
    }
    Rectangle {
        // Loading history events bar, stacked above
        // the cached events bar when more history has been requested
        anchors.right: cachedEventsBar.right
        anchors.top: chatScrollView.top
        anchors.bottom: cachedEventsBar.top
        width: cachedEventsBar.width
        visible: shuttleDial.visible

        opacity: 0.4
        color: defaultPalette.highlight
    }

    // === Scrolling extensions ===

    Slider {
        id: shuttleDial
        orientation: Qt.Vertical
        height: chatScrollView.height
        width: chatScrollView.ScrollBar.vertical.width / 2
        anchors.right: parent.right
        // Shift to the left to fit the handle in the visible area
        anchors.rightMargin: Math.max(handle.width - width, 0) / 2
        anchors.verticalCenter: chatScrollView.verticalCenter
        enabled: settings.use_shuttle_dial
        visible: enabled && chatView.count > 0

        background: Item { /* no background */ }
        handle.opacity: scrollerArea.containsMouse ? 1 : 0.7

        from: -10.0
        to: 10.0

        activeFocusOnTab: false

        onPressedChanged: {
            if (!pressed) {
                value = 0
                controller.focusInput()
            }
        }

        onValueChanged: {
            if (value)
                chatView.flick(0, parent.height * value)
        }
        Component.onCompleted: {
            // Continue scrolling while the shuttle is held out of 0
            chatView.flickEnded.connect(shuttleDial.valueChanged)
            // #375: Resume scrolling after more events arrived
            messageModel.rowsInserted.connect(shuttleDial.valueChanged)
        }
    }

    MouseArea {
        id: scrollerArea
        anchors.top: chatScrollView.top
        anchors.bottom: chatScrollView.bottom
        anchors.right: parent.right
        width: settings.use_shuttle_dial
               ? (shuttleDial.handle.width + shuttleDial.width) / 2
               : chatScrollView.ScrollBar.vertical.width
        acceptedButtons: Qt.NoButton

        hoverEnabled: true
    }

    Rectangle {
        anchors.right: scrollerArea.left
        anchors.top: chatScrollView.top
        width: childrenRect.width + 3
        height: childrenRect.height + 3
        color: defaultPalette.alternateBase
        opacity: chatView.bottommostVisibleIndex >= 0
            && (scrollerArea.containsMouse || scrollAnimation.running)
            ? 0.8 : 0
        AnimationBehavior on opacity { FastNumberAnimation { } }

        Label {
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

    ScrollToButton {
        id: scrollToBottomButton

        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: width * 1.5
        anchors.bottomMargin: visible ? 0.5 * height : -height

        visible: !chatView.atYEnd

        icon {
            name: "go-bottom"
            source: "qrc:///scrolldown.svg"
        }

        onClicked: {
            chatView.positionViewAtBeginning()
            chatView.saveViewport()
        }
    }

    ScrollToButton {
        id: scrollToReaderMarkerButton

        anchors.right: parent.right
        anchors.bottom: scrollToBottomButton.top
        anchors.rightMargin: width * 1.5
        anchors.bottomMargin: visible ? 0.5 * height : -3 * height

        visible: chatView.count > 1 &&
                 messageModel.readMarkerVisualIndex > 0 &&
                 messageModel.readMarkerVisualIndex > chatView.indexAt(chatView.contentX, chatView.contentY)

        icon {
            name: "go-top"
            source: "qrc:///scrollup.svg"
        }

        onClicked: {
            chatView.positionViewAtIndex(messageModel.readMarkerVisualIndex, ListView.Center)
            chatView.saveViewport()
        }
    }
}
