import QtQuick 2.9
import Quotient 1.0

Settings {
    readonly property int animations_duration_ms_impl:
        value("UI/animations_duration_ms", 400)
    readonly property int animations_duration_ms:
        animations_duration_ms_impl == 0 ? 10 : animations_duration_ms_impl
    readonly property int fast_animations_duration_ms: animations_duration_ms / 2

    readonly property string timeline_style: value("UI/timeline_style", "")

    // TODO: derive components from Text, Label, TextEdit, and TextArea
    // that would override font and render_type
    readonly property string font_family_impl: value("UI/Fonts/timeline_family")
    readonly property real font_pointSize_impl:
        parseFloat(value("UI/Fonts/timeline_pointSize"))
    readonly property var defaultText: Text {}
    readonly property var font: Qt.font({
        family: font_family_impl ? font_family_impl : defaultText.fontInfo.family,
        pointSize: font_pointSize_impl > 0 ? font_pointSize_impl : defaultText.fontInfo.pointSize
    })

    readonly property string render_type_impl: value("UI/Fonts/render_type")
    readonly property int render_type:
        if (["QtRendering", "Qt", "qt"].indexOf(render_type_impl) != -1)
        { Text.QtRendering }
        else if (["NativeRendering", "Native", "native"]
                 .indexOf(render_type_impl) != -1)
        { Text.NativeRendering }
        // else Qt-defined default, which is Text.QtRendering as of 5.14

}
