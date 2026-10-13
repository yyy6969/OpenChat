import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    signal switchRegister()

    background: Rectangle { color: "#f5f5f5" }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 14

        TextField { id: account; placeholderText: "账号"; Layout.preferredWidth: 220 }
        TextField { id: pwd; placeholderText: "密码"
                    echoMode: TextInput.Password; Layout.preferredWidth: 220 }

        Button {
            text: "登录"
            Layout.preferredWidth: 220
            onClicked: console.log("登录：", account.text, pwd.text)
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Button { text: "注册"; flat: true; onClicked: switchRegister() }
            Button { text: "忘记密码"; flat: true
                     onClicked: console.log("忘记密码") }
        }
    }
}
