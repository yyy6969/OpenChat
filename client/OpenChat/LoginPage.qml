import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    signal switchRegister()

    // 美化：柔和的渐变背景
    background: Rectangle {
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#F0F4F8" }
            GradientStop { position: 1.0; color: "#FFFFFF" }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 16

        // 标题美化
        Text {
            text: "OpenChat"
            font.pixelSize: 28
            font.bold: true
            font.letterSpacing: 1
            color: "#1F2937"
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 10
        }

        // 账号输入框美化
        TextField {
            id: account
            placeholderText: "账号"
            placeholderTextColor: "#9CA3AF"
            Layout.preferredWidth: 260
            Layout.preferredHeight: 25 // 保留你的高度

            leftPadding: 8 // 防止文字贴边
            color: "#374151"

            background: Rectangle {
                radius: 6
                color: account.activeFocus ? "#FFFFFF" : "#F3F4F6"
                border.color: account.activeFocus ? "#3B82F6" : "transparent"
                border.width: 1
                Behavior on color { ColorAnimation { duration: 150 } }
                Behavior on border.color { ColorAnimation { duration: 150 } }
            }
        }

        // 密码输入框美化
        TextField {
            id: pwd
            placeholderText: "密码"
            placeholderTextColor: "#9CA3AF"
            echoMode: TextInput.Password
            Layout.preferredWidth: 260
            Layout.preferredHeight: 25

            leftPadding: 8
            color: "#374151"

            background: Rectangle {
                radius: 6
                color: pwd.activeFocus ? "#FFFFFF" : "#F3F4F6"
                border.color: pwd.activeFocus ? "#3B82F6" : "transparent"
                border.width: 1
                Behavior on color { ColorAnimation { duration: 150 } }
                Behavior on border.color { ColorAnimation { duration: 150 } }
            }
        }

        // 忘记密码
        Button {
            text: "忘记密码"
            flat: true
            Layout.alignment: Qt.AlignRight
            Layout.rightMargin: 4

            contentItem: Text {
                text: parent.text
                color: parent.hovered ? "#3B82F6" : "#6B7280"
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Item {} // 去除默认背景
            onClicked: console.log("忘记密码")
        }

        // 登录和注册按钮
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 10
            spacing: 20

            // 登录按钮（主按钮）
            Button {
                text: "登录"
                Layout.preferredWidth: 100
                Layout.preferredHeight: 40

                background: Rectangle {
                    radius: 6
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: parent.pressed ? "#2563EB" : (parent.hovered ? "#60A5FA" : "#3B82F6") }
                        GradientStop { position: 1.0; color: parent.pressed ? "#1D4ED8" : (parent.hovered ? "#3B82F6" : "#2563EB") }
                    }
                    Behavior on color { ColorAnimation { duration: 150 } }
                }
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: console.log("登录：", account.text, pwd.text)
            }

            // 注册按钮（次按钮）
            Button {
                text: "注册"
                Layout.preferredWidth: 100
                Layout.preferredHeight: 40

                background: Rectangle {
                    radius: 6
                    color: parent.pressed ? "#E5E7EB" : (parent.hovered ? "#F3F4F6" : "#FFFFFF")
                    border.color: parent.hovered ? "#9CA3AF" : "#D1D5DB"
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: 150 } }
                }
                contentItem: Text {
                    text: parent.text
                    color: "#374151"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: switchRegister()
            }
        }
    }
}
