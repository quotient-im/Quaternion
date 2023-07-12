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
    readonly property bool timelineStyleIsXChat: timeline_style === "xchat"

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
    /// 2 text line heights by default; 1 line height for XChat
    readonly property real minimalTimelineItemHeight:
        lineSpacing * (2 - timelineStyleIsXChat)

    readonly property var render_type_impl: value("UI/Fonts/render_type",
                                                  "NativeRendering")
    readonly property int render_type:
        ["NativeRendering", "Native", "native"].indexOf(render_type_impl) != -1
        ? Text.NativeRendering : Text.QtRendering
    readonly property bool use_shuttle_dial: value("UI/use_shuttle_dial", true)
    readonly property bool autoload_images: value("UI/autoload_images", true)

    readonly property var disabledPalette:
        SystemPalette { colorGroup: SystemPalette.Disabled }

    function mixColors(baseColor, mixedColor, mixRatio)
    {
        return Qt.tint(baseColor,
                Qt.rgba(mixedColor.r, mixedColor.g, mixedColor.b, mixRatio))
    }

    readonly property string highlight_mode: value("UI/highlight_mode", "background")
    readonly property color highlight_color: value("UI/highlight_color", "orange")

    readonly property color lowlight_color: mixColors(disabledPalette.text,
                                                      palette.text, 0.3)
    readonly property bool show_author_avatars:
        value("UI/show_author_avatars", !timelineStyleIsXChat)
}
