import QtQuick
import QtQuick.Controls

Page {
    header: ToolBar {
        Button { text: "← 返回"; onClicked: StackView.view.pop() }
    }
    Label { anchors.centerIn: parent; text: "注册界面" }
}
