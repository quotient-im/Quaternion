import QtQuick 2.15

Timer {
    property int targetIndex: -1
    property var targetPos
    property int positionMode: ListView.End
    property int round: 1

    readonly property var lc: parent.lc
    readonly property real contentX: parent.contentX
    readonly property real contentY: parent.contentY
    readonly property real height: parent.height

    /** @brief Scroll to the position making sure the timeline is actually at it
      *
      * Qt is not fabulous at positioning the list view when the delegate
      * sizes vary too much; this function runs scrollFinisher timer to adjust
      * the position as needed shortly after the list was positioned.
      * Nothing good in that, just a workaround.
      *
      * This function is the entry point to get the whole component do its job.
      */
    function scrollViewTo(newTargetIndex, newPositionMode) {
        targetIndex = newTargetIndex
        positionMode = newPositionMode

        if (targetPos) {
            // Target coordinate already known, do a nice animated scroll to it
            console.log(lc, "Scrolling to item", newTargetIndex)
            parent.animateNextScroll = true
            adjustPosition()
        } else {
            // The target item is not loaded yet; use ListView's imprecise
            // positioning and fixup upon arrival
            console.log(lc, "Jumping to item", newTargetIndex)
            parent.positionViewAtIndex(newTargetIndex, newPositionMode)
            round = 1
            start()
        }
    }

    function bindScrollTarget(item) {
        targetPos = Qt.binding(() => { return item.y })
        console.log(lc, "Scroll target bound, current pos:", targetPos)
    }

    /** @brief Bind the target position to the scroll target's ordinate
      *
      * This should be called from each TimelineItem component so that
      * the finisher could detect when the timeline item with `targetIndex` is
      * created, and try to scroll to its `y` position. Because the item might
      * not be perfectly positioned at the moment of creation, it may take
      * an additional iteration or two before the finisher sets the timeline
      * on the right position.
      */
    function maybeBindScrollTarget(delegate) {
        if (targetIndex === delegate.index)
            bindScrollTarget(delegate)
    }

    function logFixup(nameForLog, topContentY, bottomContentY) {
        var topShownIndex = parent.indexAt(contentX, topContentY)
        var bottomShownIndex = parent.indexAt(contentX, bottomContentY)
        if (bottomShownIndex !== -1 && targetIndex <= topShownIndex
                && targetIndex >= bottomShownIndex)
            return true // The item is within the expected range

        console.log(lc, "Fixing up item", targetIndex, "to be", nameForLog,
                    "- fixup round #" + scrollFinisher.round,
                    "(" + topShownIndex + "-" + bottomShownIndex,
                    "range is shown now)")
    }

    /** @return true if no further action is needed;
     *          false if the finisher has to be restarted.
     */
    function adjustPosition() {
        if (targetIndex === 0) {
            if (parent.bottommostVisibleIndex === 0)
                return true // Positioning is correct

            // This normally shouldn't happen even with the current
            // imperfect positioning code in Qt
            console.warn(lc, "Fixing up the viewport to be at sync edge")
            parent.positionViewAtBeginning()
        } else {
            // The viewport is divided into thirds; ListView.End should
            // place targetIndex at the top third, Center corresponds
            // to the middle third; Beginning is not used for now.
            var nameForLog, topContentY, bottomContentY
            switch (positionMode) {
            case ListView.Contain:
                logFixup("fully visible", contentY, contentY + height)
                if (targetPos)
                    targetPos = Math.max(targetPos,
                                      Math.min(contentY, targetPos + height))
                break
            case ListView.Center:
                logFixup("in the centre", contentY + height / 3,
                         contentY + 2 * height / 3)
                if (targetPos)
                    targetPos -= height / 2
                break
            case ListView.End:
                logFixup("at the top", contentY, contentY + height / 3)
                break
            default:
                console.warn(lc, "fixupPosition: Unsupported positioning mode:",
                             positionMode)
                return true // Refuse to do anything with it
            }

            // If the target item moved away too far and got destroyed,
            // repeat positioning; otherwise, position the canvas exactly
            // where it should be
            if (targetPos) {
                // this.contentY is readonly
                parent.contentY = targetPos
                return true
            }
            parent.positionViewAtIndex(targetIndex, positionMode)
        }
        return false
    }

    interval: 120 // small enough to avoid visual stutter
    onTriggered: {
        if (parent.count === 0 || !targetPos)
            return

        if (adjustPosition() || ++round > 3) { // Give up after 3 rounds
            targetPos = undefined
            parent.saveViewport(true)
        } else // Positioning is still in flux, might need another round
            start()
    }
    onTargetIndexChanged: {
        var item = parent.itemAtIndex(targetIndex)
        if (item)
            bindScrollTarget(item)
    }
}
