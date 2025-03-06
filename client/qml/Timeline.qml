import QtQuick 2.15
import QtQuick.Controls 2.3
import Quotient 1.0

Page {
    id: root

    property Room room: messageModel?.room

    readonly property Logger lc: Logger { }
    TimelineSettings {
        id: settings

        Component.onCompleted: console.log(root.lc, "Using timeline font: " + font)
    }

    background: Rectangle { color: palette.base; border.color: palette.mid }
    contentWidth: width
    font: settings.font

    header: RoomHeader { room: root.room }

    ListView {
        id: chatView
        anchors.fill: parent

        model: messageModel
        delegate: TimelineItem {
            width: chatView.width - scrollerArea.width
            // #737; the solution found in https://bugreports.qt.io/browse/QT3DS-784
            ListView.delayRemove: true
        }
        verticalLayoutDirection: ListView.BottomToTop
        flickableDirection: Flickable.VerticalFlick
        flickDeceleration: 8000
        boundsMovement: Flickable.StopAtBounds
//        pixelAligned: true // Causes false-negatives in atYEnd
        cacheBuffer: 200

        clip: true
        ScrollBar.vertical: ScrollBar {
            policy: settings.use_shuttle_dial ? ScrollBar.AlwaysOff : ScrollBar.AsNeeded
            interactive: true
            active: true
//            background: Item { /* TODO: timeline map */ }
        }

        // We do not actually render sections because section delegates return
        // -1 for indexAt(), disrupting quite a few things including read marker
        // logic, saving the current position etc. Besides, ListView sections
        // cannot be effectively nested. TimelineItem.qml implements
        // the necessary  logic around eventGrouping without these shortcomings.
        section.property: "date"

        readonly property int bottommostVisibleIndex: count > 0 ?
            atYEnd ? 0 : indexAt(contentX, contentY + height - 1) : -1
        readonly property bool noNeedMoreContent:
            !root.room || root.room.eventsHistoryJob || root.room.allHistoryLoaded

        /// The number of events per height unit - always positive
        readonly property real eventDensity:
            contentHeight > 0 && count > 0 ? count / contentHeight : 0.03
            // 0.03 is just an arbitrary reasonable number

        property var textEditWithSelection
        property real readMarkerContentPos: originY
        readonly property real readMarkerViewportPos:
            readMarkerContentPos < contentY ? 0 :
            readMarkerContentPos > contentY + height ? height + readMarkerLine.height :
            readMarkerContentPos - contentY

        function parkReadMarker() {
            readMarkerContentPos = Qt.binding(function() {
                return messageModel.readMarkerVisualIndex <= indexAt(contentX, contentY)
                       ? contentY + contentHeight : originY
            })
        }

        function ensurePreviousContent() {
            if (noNeedMoreContent)
                return

            // Take the current speed, or assume we can scroll 8 screens/s
            var velocity = moving ? -verticalVelocity
                                  : cruisingAnimation.running ? cruisingAnimation.velocity
                                                              : chatView.height * 8
            // Check if we're about to bump into the ceiling in
            // 2 seconds and if yes, request the amount of messages
            // enough to scroll at this rate for 3 more seconds
            if (velocity > 0 && contentY - velocity*2 < originY)
                root.room.getPreviousContent(velocity * eventDensity * 3)
        }
        onContentYChanged: ensurePreviousContent()
        onContentHeightChanged: ensurePreviousContent()

        function saveViewport(force) {
            root.room?.saveViewport(indexAt(contentX, contentY), bottommostVisibleIndex, force)
        }

        ScrollFinisher { id: scrollFinisher }

        function scrollUp(dy) {
            if (contentHeight > height && dy !== 0) {
                animateNextScroll = true
                contentY -= dy
            }
        }
        function scrollDown(dy) { scrollUp(-dy) }

        function onWheel(wheel) {
            if (wheel.angleDelta.x === 0) {
                // NB: Scrolling up yields positive angleDelta.y
                if (contentHeight > height && wheel.angleDelta.y !== 0)
                    contentY -= wheel.angleDelta.y * settings.lineSpacing / 40
                wheel.accepted = true
            } else {
                wheel.accepted = false
            }
        }

        Connections {
            target: controller
            function onPageUpPressed() {
                chatView.scrollUp(chatView.height - sectionBanner.childrenRect.height)
            }
            function onPageDownPressed() {
                chatView.scrollDown(chatView.height - sectionBanner.childrenRect.height)
            }
            function onViewPositionRequested(index) {
                scrollFinisher.scrollViewTo(index, ListView.Contain)
            }
            function onHistoryRequestChanged() {
                scrollToReadMarkerButton.checked = controller.isHistoryRequestRunning()
            }
        }

        Connections {
            target: messageModel
            function onModelAboutToBeReset() {
                chatView.parkReadMarker()
                console.log(lc, "Read marker parked at index", messageModel.readMarkerVisualIndex)
                chatView.saveViewport(true)
            }
            function onModelReset() {
                // NB: at this point, the actual delegates are not instantiated
                //     yet, so defer all actions to when at least some are
                scrollFinisher.scrollViewTo(0, ListView.Contain)
            }
        }

        Component.onCompleted: console.log(root.lc, "QML view loaded")

        onMovementEnded: saveViewport(false)

        populate: AnimatedTransition { FastNumberAnimation { property: "opacity"; from: 0; to: 1 } }

        add: AnimatedTransition { FastNumberAnimation { property: "opacity"; from: 0; to: 1 } }

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

        property bool animateNextScroll: false
        Behavior on contentY {
            enabled: settings.enable_animations && chatView.animateNextScroll
            animation: FastNumberAnimation {
                id: scrollAnimation
                duration: settings.fast_animations_duration_ms / 3
                onStopped: {
                    chatView.animateNextScroll = false
                    chatView.saveViewport(false)
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

        readonly property color readMarkerColor: palette.highlight

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
            width: readMarkerLine.width
            z: -1
            opacity: 0.05

            radius: readMarkerLine.height
            color: chatView.readMarkerColor
        }
        Rectangle {
            id: readMarkerLine

            visible: chatView.count > 0
            width: parent.width - scrollerArea.width
            anchors.bottom: readShade.bottom
            height: 4
            z: 2.5 // On top of any ListView content, below the banner

            gradient: Gradient {
                GradientStop { position: 0; color: "transparent" }
                GradientStop { position: 1; color: chatView.readMarkerColor }
            }
        }

        // itemAt is a function rather than a property, so it doesn't
        // produce a QML binding; the piece with contentHeight compensates.
        readonly property var underlayingItem: contentHeight >= height
            ? itemAt(contentX, contentY + sectionBanner.height - 2) : undefined
        readonly property bool sectionBannerVisible: !!underlayingItem &&
            (!underlayingItem.sectionVisible || underlayingItem.y < contentY)

        Rectangle {
            id: sectionBanner
            z: 3 // On top of ListView sections that have z=2
            anchors.left: parent.left
            anchors.top: parent.top
            width: childrenRect.width + 2
            height: childrenRect.height + 2
            visible: chatView.sectionBannerVisible
            color: palette.window
            opacity: 0.8
            Label {
                font.bold: true
                opacity: 0.8
                renderType: settings.render_type
                text: chatView.underlayingItem?.ListView.section ?? ""
            }
        }
    }

    // === Timeline map ===
    // Only used with the shuttle scroller for now

    Rectangle {
        id: cachedEventsBar

        // A proxy property for animation
        property int requestedHistoryEventsCount: root.room?.requestedHistorySize ?? 0
        AnimationBehavior on requestedHistoryEventsCount { NormalNumberAnimation { } }

        property real averageEvtHeight:
            chatView.count + requestedHistoryEventsCount > 0
            ? chatView.height / (chatView.count + requestedHistoryEventsCount) : 0
        AnimationBehavior on averageEvtHeight { FastNumberAnimation { } }

        anchors.horizontalCenter: shuttleDial.horizontalCenter
        anchors.bottom: chatView.bottom
        anchors.bottomMargin:
            averageEvtHeight * chatView.bottommostVisibleIndex
        width: shuttleDial.width
        height: chatView.bottommostVisibleIndex < 0
                ? 0 : averageEvtHeight * (chatView.count - chatView.bottommostVisibleIndex)
        visible: shuttleDial.visible

        color: palette.mid
    }
    Rectangle {
        // Loading history events bar, stacked above
        // the cached events bar when more history has been requested
        anchors.right: cachedEventsBar.right
        anchors.top: chatView.top
        anchors.bottom: cachedEventsBar.top
        width: cachedEventsBar.width
        visible: shuttleDial.visible

        opacity: 0.4
        color: palette.mid
    }

    // === Scrolling extensions ===

    Slider {
        id: shuttleDial
        orientation: Qt.Vertical
        height: chatView.height * 0.7
        width: settings.lineSpacing
        padding: 2
        anchors.right: parent.right
        anchors.verticalCenter: chatView.verticalCenter
        enabled: settings.use_shuttle_dial
        visible: enabled && chatView.count > 0

        // Npages/sec = value^2 => maxNpages/sec = 9
        readonly property real maxValue: 3.0
        readonly property real handlePos:
            topPadding + visualPosition * (availableHeight - handle.height)

        from: -maxValue
        to: maxValue

        background: Rectangle {
            // Draw a "spring" line between the shuttle and the center of the control
            anchors.top: (shuttleDial.value > 0 ? shuttleHandle : shuttleDial).verticalCenter
            anchors.bottom: (shuttleDial.value > 0 ? shuttleDial : shuttleHandle).verticalCenter
            anchors.horizontalCenter: parent.horizontalCenter
            width: shuttleDial.availableWidth
            radius: 2
            color: palette.highlight
        }

        handle: Rectangle {
            id: shuttleHandle
            y: shuttleDial.handlePos
            width: shuttleDial.availableWidth
            anchors.horizontalCenter: shuttleDial.horizontalCenter
            height: width * 1.618
            radius: width
            color: palette.button
            border.color: scrollerArea.containsMouse ? palette.highlight : palette.button
        }

        opacity: scrollerArea.containsMouse ? 1 : 0.7
        AnimationBehavior on opacity { FastNumberAnimation { } }

        activeFocusOnTab: false

        onPressedChanged: {
            if (!pressed) {
                value = 0
                controller.focusInput()
            }
        }

        // This is not an ordinary animation, it's the engine that makes
        // the shuttle dial work; for that reason it's not governed by
        // settings.enable_animations and only can be disabled together with
        // the shuttle dial.
        SmoothedAnimation {
            id: cruisingAnimation
            target: chatView
            property: "contentY"
            velocity: shuttleDial.value * shuttleDial.value * chatView.height
            maximumEasingTime: settings.animations_duration_ms
            to: chatView.originY + (shuttleDial.value > 0 ? 0 :
                    chatView.contentHeight - chatView.height)
            running: shuttleDial.value != 0

            onStopped: chatView.saveViewport(false)
        }

        // Animations don't update `to` value when they are running; so
        // when the shuttle value changes sign without becoming zero (which,
        // turns out, is quite usual when dragging the shuttle around) the
        // animation has to be restarted.
        onValueChanged: cruisingAnimation.restart()
        Component.onCompleted: { // same reason as above
            chatView.originYChanged.connect(cruisingAnimation.restart)
            chatView.contentHeightChanged.connect(cruisingAnimation.restart)
        }
    }

    MouseArea {
        id: scrollerArea
        anchors.verticalCenter: chatView.verticalCenter
        anchors.right: parent.right
        width: (settings.use_shuttle_dial ? shuttleDial : chatView.ScrollBar.vertical).width
        height: (settings.use_shuttle_dial ? shuttleDial : chatView).height
        acceptedButtons: Qt.NoButton

        hoverEnabled: true
    }

    Rectangle {
        id: timelineStats
        anchors.right: scrollerArea.left
        anchors.top: chatView.top
        width: childrenRect.width
        height: childrenRect.height
        color: palette.alternateBase
        opacity: 0 // Nothing to show at the start
        property bool shown: (chatView.bottommostVisibleIndex >= 0
                              && (scrollerArea.containsMouse || scrollAnimation.running))
                             || root.room?.requestedHistorySize > 0
        onShownChanged: {
            if (shown) {
                fadeOutDelay.stop()
                opacity = 0.8
            } else
                fadeOutDelay.restart()
        }
        Timer {
            id: fadeOutDelay
            interval: 2000
            onTriggered: parent.opacity = 0
        }

        AnimationBehavior on opacity { FastNumberAnimation { } }

        Label {
            padding: 2
            font.bold: true
            opacity: 0.8
            renderType: settings.render_type
            text: (chatView.count > 0
                   ? (chatView.bottommostVisibleIndex === 0
                     ? qsTr("Latest events")
                     : qsTr("%Ln events back from now","", chatView.bottommostVisibleIndex))
                     + "\n" + qsTr("%Ln events cached", "", chatView.count)
                   : "")
                  + (root.room?.requestedHistorySize > 0
                     ? (chatView.count > 0 ? "\n" : "")
                       + qsTr("%Ln events requested from the server", "", room.requestedHistorySize)
                     : "")
            horizontalAlignment: Label.AlignRight
        }
    }

    component ScrollToButton: Button {
        id: control
        anchors.right: scrollerArea.right
        height: settings.fontHeight * 2
        width: scrollerArea.width
        hoverEnabled: true
        opacity: visible * (0.7 + hovered * 0.2)

        display: Button.IconOnly
        icon {
            width: control.availableWidth
            height: control.availableHeight
            color: palette.buttonText
        }

        AnimationBehavior on opacity {
            NormalNumberAnimation { easing.type: Easing.OutQuad }
        }
        AnimationBehavior on anchors.topMargin {
            NormalNumberAnimation { easing.type: Easing.OutQuad }
        }
        AnimationBehavior on anchors.bottomMargin {
            NormalNumberAnimation { easing.type: Easing.OutQuad }
        }
    }

    ScrollToButton {
        id: scrollToBottomButton

        anchors.bottom: chatView.bottom
        anchors.bottomMargin: visible ? 0 : -height

        visible: !chatView.atYEnd

        icon {
            name: "go-bottom"
            source: "qrc:///scrolldown.svg"
        }

        onClicked: {
            chatView.positionViewAtBeginning()
            chatView.saveViewport(true)
        }
    }

    ScrollToButton {
        id: scrollToReadMarkerButton

        anchors.top: parent.top
        anchors.topMargin: visible ? 0.5 * height : -height

        visible: chatView.count > 1 &&
                 messageModel.readMarkerVisualIndex > 0 &&
                 messageModel.readMarkerVisualIndex
                    > chatView.indexAt(chatView.contentX, chatView.contentY)

        icon {
            name: "go-top"
            source: "qrc:///scrollup.svg"
        }

        onClicked: {
            if (messageModel.readMarkerVisualIndex < chatView.count)
                scrollFinisher.scrollViewTo(messageModel.readMarkerVisualIndex,
                                            ListView.Center)
            else {
                checkable = true
                controller.ensureLastReadEvent()
            }
        }
        onCheckedChanged: { if (!checked) checkable = false }
    }
}
