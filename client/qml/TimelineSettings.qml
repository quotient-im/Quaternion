import QtQuick 2.4
import Quotient 1.0

Settings {
    readonly property int animations_duration_ms_impl:
        value("UI/animations_duration_ms", 400)
    readonly property bool enable_animations: animations_duration_ms_impl > 0
    readonly property int animations_duration_ms:
        animations_duration_ms_impl == 0 ? 10 : animations_duration_ms_impl
    readonly property int fast_animations_duration_ms: animations_duration_ms / 2

    readonly property string timeline_style: value("UI/timeline_style", "")

    readonly property string font_family_impl:
        value("UI/Fonts/timeline_family", "")
    readonly property real font_pointSize_impl:
        parseFloat(value("UI/Fonts/timeline_pointSize", ""))
    readonly property var defaultMetrics: FontMetrics { }
    readonly property var fontInfo: FontMetrics {
        font.family: font_family_impl ? font_family_impl
                                      : defaultMetrics.font.family
        font.pointSize: font_pointSize_impl > 0 ? font_pointSize_impl
                                                : defaultMetrics.font.pointSize
    }
    readonly property var font: fontInfo.font
    readonly property real fontHeight: fontInfo.height
    readonly property real lineSpacing: fontInfo.lineSpacing

    readonly property var render_type_impl: value("UI/Fonts/render_type",
                                                  "NativeRendering")
    readonly property int render_type:
        ["NativeRendering", "Native", "native"].indexOf(render_type_impl) != -1
        ? Text.NativeRendering : Text.QtRendering
    readonly property bool use_shuttle_dial: value("UI/use_shuttle_dial", true)
    readonly property bool autoload_images: value("UI/autoload_images", true)
    readonly property string highlight_mode: value("UI/highlight_mode", "background")
    readonly property color highlight_color: value("UI/highlight_color", "orange")
    readonly property color outgoing_color_base: value("UI/outgoing_color", "#4A8780")
    readonly property color outgoing_color:
        mixColors(defaultPalette.text, settings.outgoing_color_base, 0.5)
    readonly property bool show_author_avatars:
        value("UI/show_author_avatars", !timelineStyleIsXChat)
}
