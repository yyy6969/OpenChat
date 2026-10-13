import QtQuick
import QtQuick.Controls

ApplicationWindow {
    width: 350; height: 500
    visible: true
    title: "OpenChat"

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: LoginPage {}
    }

    Connections {
        target: stack.currentItem
        function onSwitchRegister() { stack.push("RegisterPage.qml") }
    }
}
