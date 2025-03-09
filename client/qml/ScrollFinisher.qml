import QtQuick 2.15

Timer {
    property int targetIndex: -1
    property int positionMode: ListView.End
    property int round: 1

    readonly property var lc: root.lc

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
        console.log(lc, "Jumping to item", newTargetIndex)
        parent.animateNextScroll = true
        parent.positionViewAtIndex(newTargetIndex, newPositionMode)
        targetIndex = newTargetIndex
        positionMode = newPositionMode
        round = 1
        start()
    }

    function logFixup(nameForLog, topContentY, bottomContentY) {
        const contentX = parent.contentX
        const topShownIndex = parent.indexAt(contentX, topContentY)
        const bottomShownIndex = parent.indexAt(contentX, bottomContentY - 1)
        if (bottomShownIndex !== -1
                && (targetIndex > topShownIndex || targetIndex < bottomShownIndex))
            console.log(lc, "Fixing up item", targetIndex, "to be", nameForLog,
                        "- fixup round #" + scrollFinisher.round,
                        "(" + topShownIndex + "-" + bottomShownIndex,
                        "range is shown now)")
    }

    /** @return true if no further action is needed; false if the finisher has to be restarted. */
    function adjustPosition() {
        if (targetIndex === 0) {
            if (parent.bottommostVisibleIndex === 0)
                return true // Positioning is correct

            // This normally shouldn't happen even with the current
            // imperfect positioning code in Qt
            console.warn(lc, "Fixing up the viewport to be at sync edge")
            parent.positionViewAtBeginning()
        } else {
            const height = parent.height
            const contentY = parent.contentY
            const viewport1stThird = contentY + height / 3
            const item = parent.itemAtIndex(targetIndex)
            if (item) {
                // The viewport is divided into thirds; ListView.End should
                // place targetIndex at the top third, Center corresponds
                // to the middle third; Beginning is not used for now.
                switch (positionMode) {
                case ListView.Contain:
                    if (item.y >= contentY || item.y + item.height < contentY + height)
                        return true // Positioning successful
                    logFixup("fully visible", contentY, contentY + height)
                    break
                case ListView.Center:
                    const viewport2ndThird = contentY + 2 * height / 3
                    const itemMidPoint = item.y + item.height / 2
                    if (itemMidPoint >= viewport1stThird && itemMidPoint < viewport2ndThird)
                        return true
                    logFixup("in the centre", viewport1stThird, viewport2ndThird)
                    break
                case ListView.End:
                    if (item.y >= contentY && item.y < viewport1stThird)
                        return true
                    logFixup("at the top", contentY, viewport1stThird)
                    break
                default:
                    console.warn(lc, "fixupPosition: Unsupported positioning mode:", positionMode)
                    return true // Refuse to do anything with it
                }
            }
            // If the target item moved away too far and got destroyed, repeat positioning
            parent.animateNextScroll = true
            parent.positionViewAtIndex(targetIndex, positionMode)
            return false
        }
    }

    interval: 120 // small enough to avoid visual stutter
    onTriggered: {
        if (parent.count === 0)
            return

        if (adjustPosition() || ++round > 3 /* Give up after 3 rounds */) {
            targetIndex = -1
            parent.saveViewport(true)
        } else // Positioning is still in flux, might need another round
            start()
    }
}
