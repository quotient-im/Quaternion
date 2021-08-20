import QtQuick 2.9
import QtQuick.Controls 2.2

RoundButton {
    height: settings.fontHeight * 2
    width: height
    hoverEnabled: true
    opacity: visible * (0.7 + hovered * 0.2)

    display: Button.IconOnly
    icon.color: defaultPalette.buttonText

    AnimationBehavior on opacity {
        NormalNumberAnimation {
            easing.type: Easing.OutQuad
        }
    }
    AnimationBehavior on anchors.bottomMargin {
        NormalNumberAnimation {
            easing.type: Easing.OutQuad
        }
    }
}
