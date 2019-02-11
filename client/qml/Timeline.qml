import QtQuick 2.2
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.0
import QtQuick.Layouts 1.1
import QMatrixClient 1.0

Rectangle {
    id: root

    Settings {
        id: settings
        readonly property bool autoload_images: value("UI/autoload_images", true)
        readonly property string render_type: value("UI/Fonts/render_type", "NativeRendering")
        readonly property int animations_duration_ms: value("UI/animations_duration_ms", 400)
        readonly property int fast_animations_duration_ms: animations_duration_ms / 2
        readonly property bool use_shuttle_dial: value("UI/use_shuttle_dial", true)
    }
    SystemPalette { id: defaultPalette; colorGroup: SystemPalette.Active }
    SystemPalette { id: disabledPalette; colorGroup: SystemPalette.Disabled }

    color:  defaultPalette.base

    function humanSize(bytes)
    {
        if (!bytes)
            return qsTr("Unknown", "Unknown attachment size")
        if (bytes < 4000)
            return qsTr("%1 bytes").arg(bytes)
        bytes = Math.round(bytes / 100) / 10
        if (bytes < 2000)
            return qsTr("%1 KB").arg(bytes)
        bytes = Math.round(bytes / 100) / 10
        if (bytes < 2000)
            return qsTr("%1 MB").arg(bytes)
        return qsTr("%1 GB").arg(Math.round(bytes / 100) / 10)
    }

    ScrollView {
        id: chatScrollView
        anchors.fill: parent
        anchors.rightMargin: if (settings.use_shuttle_dial) { shuttleDial.width }
        horizontalScrollBarPolicy: Qt.ScrollBarAlwaysOff
        verticalScrollBarPolicy: settings.use_shuttle_dial
                                 ? Qt.ScrollBarAlwaysOff : Qt.ScrollBarAlwaysOn
        style: ScrollViewStyle { transientScrollBars: true }

        ListView {
            id: chatView

            model: messageModel
            delegate: TimelineItem {
                width: chatView.width
                view: chatView
                moving: chatView.moving || shuttleDial.value
            }
            verticalLayoutDirection: ListView.BottomToTop
            flickableDirection: Flickable.VerticalFlick
            flickDeceleration: 10000
            boundsBehavior: Flickable.StopAtBounds
    //        pixelAligned: true
            cacheBuffer: 200

            section { property: "section" }

            property int largestVisibleIndex: count > 0 ?
                indexAt(contentX, contentY + height - 1) : -1
            readonly property bool loadingHistory:
                room ? room.eventsHistoryJob : false

            onLoadingHistoryChanged:
                console.log("loadingHistory="+loadingHistory)

            function ensurePreviousContent() {
                if (loadingHistory)
                    return
                // Snapshot the current speed, or assume we scroll 10 screens/s
                var curVelocity = moving ? -verticalVelocity : height * 10
                // Check if we're about to bump into the ceiling in 2 seconds
                if (curVelocity > 0 && contentY - curVelocity*2 < originY)
                {
                    // Request the amount of messages enough to scroll
                    // at this rate for 3 more seconds
                    var avgEventHeight = contentHeight / count
                    room.getPreviousContent(curVelocity*3 / avgEventHeight);
                }
            }

            function saveViewport() {
                room.saveViewport(indexAt(contentX, contentY),
                                  largestVisibleIndex)
            }

            function onModelAboutToReset() {
                console.log("Resetting timeline model")
                contentYChanged.disconnect(ensurePreviousContent)
            }

            function onModelReset() {
                if (room)
                {
                    var lastScrollPosition = room.savedTopVisibleIndex()
                    contentYChanged.connect(ensurePreviousContent)
                    if (lastScrollPosition === 0)
                        positionViewAtBeginning()
                    else
                    {
                        console.log("Scrolling to position", lastScrollPosition)
                        positionViewAtIndex(lastScrollPosition, ListView.Contain)
                    }
                    if (contentY < originY + 10)
                        room.getPreviousContent(100)
                }
                console.log("Model timeline reset")
            }

            Component.onCompleted: {
                console.log("QML view loaded")
                model.modelAboutToBeReset.connect(onModelAboutToReset)
                model.modelReset.connect(onModelReset)
            }

            onMovementEnded: saveViewport()

            displaced: Transition { NumberAnimation {
                property: "y"; duration: settings.fast_animations_duration_ms
                easing.type: Easing.OutQuad
            }}

            Behavior on contentY {
                enabled: !chatView.moving
                SmoothedAnimation {
                    id: scrollAnimation
                    duration: settings.fast_animations_duration_ms
                    maximumEasingTime: settings.animations_duration_ms

                    onRunningChanged: { if (!running) chatView.saveViewport() }
            }}

            // itemAt is a function, not a property so is not bound to new items
            // showing up underneath; contentHeight is used for that instead.
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
                opacity: 0.9
                Label {
                    font.bold: true
                    color: disabledPalette.text
                    renderType: settings.render_type
                    text: chatView.underlayingItem ?
                              chatView.underlayingItem.ListView.section : ""
                }
            }
        }
    }
    Slider {
        id: shuttleDial
        orientation: Qt.Vertical
        height: chatScrollView.height
        anchors.right: parent.right
        anchors.verticalCenter: chatScrollView.verticalCenter
        enabled: settings.use_shuttle_dial
        visible: enabled

        style: SliderStyle {
            // Width and height are swapped below because SliderStyle assumes
            // a horizontal slider - but anchors are not :-\
            groove: Rectangle {
                color: defaultPalette.window
                border.color: defaultPalette.midlight
                implicitHeight: 8
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    implicitHeight: 2
                    width: chatView.largestVisibleIndex < 0 ? 0 :
                        chatView.height *
                            (1 - chatView.largestVisibleIndex / chatView.count)

                    color: defaultPalette.highlight
                }
            }
            handle: Rectangle {
                anchors.centerIn: parent
                color: defaultPalette.button
                border.color: defaultPalette.buttonText
                border.width: 1
                implicitWidth: 14
                implicitHeight: 8
                visible: chatView.count > 0
            }
        }

        maximumValue: 10.0
        minimumValue: -10.0

        activeFocusOnPress: false
        activeFocusOnTab: false
        // wheelEnabled: false // Only available in QQC 1.6, Qt 5.10

        onPressedChanged: {
            if (!pressed)
                value = 0
        }

        onValueChanged: {
            if (value)
                chatView.flick(0, parent.height * value)
        }
        Component.onCompleted: {
            // Continue scrolling while the shuttle is held out of 0
            chatView.flickEnded.connect(shuttleDial.valueChanged)
        }
    }

    MouseArea {
        id: scrollerArea
        anchors.top: chatScrollView.top
        anchors.bottom: chatScrollView.bottom
        anchors.right: parent.right
        width: settings.use_shuttle_dial ? shuttleDial.width
                                         : chatScrollView.width - chatView.width
        acceptedButtons: Qt.NoButton
        // FIXME: propagate wheel event to chatView
        onWheel: { wheel.accepted = settings.use_shuttle_dial }

        hoverEnabled: true
    }

    Rectangle {
        anchors.right: scrollerArea.left
        anchors.top: chatScrollView.top
        width: childrenRect.width + 3
        height: childrenRect.height + 3
        visible: chatView.largestVisibleIndex >= 0 &&
                 (scrollerArea.containsMouse || scrollAnimation.running)
        color: defaultPalette.window
        opacity: 0.9
        Label {
            font.bold: true
            color: disabledPalette.text
            renderType: settings.render_type
            text: qsTr("%Ln events back from now (%L1 cached%2)",
                       "%2 is optional 'and loading'",
                       chatView.largestVisibleIndex)
                  .arg(chatView.count)
                  .arg(chatView.loadingHistory ? (" " + qsTr("and loading"))
                                               : "")
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
        Behavior on opacity { NumberAnimation {
            duration: settings.animations_duration_ms
            easing.type: Easing.OutQuad
        }}
        Behavior on anchors.bottomMargin { NumberAnimation {
            duration: settings.animations_duration_ms
            easing.type: Easing.OutQuad
        }}
        Image {
            anchors.fill: parent
            source: "qrc:///scrolldown.svg"
        }
        MouseArea {
            anchors.fill: parent
            onClicked: chatView.positionViewAtBeginning()
            cursorShape: Qt.PointingHandCursor
        }
    }
}
