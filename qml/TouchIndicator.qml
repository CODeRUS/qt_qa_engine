import QtQuick 2.0

Item {
    id: root
    width: 1
    height: 1
    opacity: 0.0
    property int size: 24

    function show(point, psize) {
        hideAnimation.stop()
        x = point.x
        y = point.y
        if (psize && psize.width > 0) {
            size = psize.width
        } else {
            size = 24
        }
        opacity = 1.0
    }

    function hide() {
        hideAnimation.start()
    }

    function hideImmediately() {
        hideAnimation.stop()
        opacity = 0.0
    }

    Rectangle {
        anchors.centerIn: parent
        width: root.size
        height: root.size
        radius: root.size / 2
        border.color: "#400000000"
        border.width: 1
        color: "#40ffffff"
    }

    NumberAnimation {
        id: hideAnimation
        target: root
        property: "opacity"
        to: 0.0
        duration: 250
        easing.type: Easing.InQuad
    }
}
