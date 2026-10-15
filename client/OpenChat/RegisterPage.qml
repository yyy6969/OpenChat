import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    signal switchLogin()

    //渐变色
    background: Rectangle {
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#F0F4F8" }
            GradientStop { position: 1.0; color: "#FFFFFF" }
        }
    }

    property var handlers: ({
           1001: onGetVerifyCode,      // ReqId::ID_GET_VERIFY_CODE
           1002: onRegUser             // ReqId::ID_REG_USER
       })

    function showTip(msg, ok) {
        errorText.text = msg
        errorText.color = ok ? "#10B981" : "#EF4444"
        errorText.visible = true
    }

    function onGetVerifyCode(jsonObj) {
            if (jsonObj.error !== 0) {                       // ErrorCodes::SUCCESS
                showTip(qsTr("参数错误"), false)
                return
            }
            let email = jsonObj.email
            showTip(qsTr("验证码已发送到邮箱，注意查收"), true)
            console.log("email is", email)
        }

        function onRegUser(jsonObj) {
            if (jsonObj.error !== 0) {
                showTip(qsTr("参数错误"), false)
                return
            }
            let email = jsonObj.email
            showTip(qsTr("用户注册成功"), true)
            console.log("email is", email)
            console.log("user uuid is", jsonObj.uid)
            //changeTipPage()todo
        }

    Connections {
        target: HttpMgr
        function onSig_reg_mod_finish(reqId, res, err) {

            if (err !== 0)
            {
                errorText.text = "网络请求错误（错误码 " + err + "）"
                errorText.visible = true
                return
            }
            let jsonDoc
            try {
                jsonDoc = JSON.parse(res)
            } catch (e) {
                errorText.text = "JSON解析失败"
                errorText.visible = true
                return
            }

            if (typeof jsonDoc !== "object" || jsonDoc === null || Array.isArray(jsonDoc)) {
                errorText.text = "JSON解析失败"
                errorText.visible = true
                return
            }
                handlers[reqId](jsonDoc)
        }
    }


    ColumnLayout {
        anchors.centerIn: parent
        spacing: 12

        Text {
            text: "注册账号"
            font.pixelSize: 28
            font.bold: true
            font.letterSpacing: 1
            color: "#1F2937"
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 10
        }

        // 一个内联的组件，减少重复代码
        component ModernTextField : TextField {
            id: control
            placeholderTextColor: "#9CA3AF"
            Layout.preferredWidth: 260
            Layout.preferredHeight: 25
            leftPadding: 8
            color: "#374151"
            background: Rectangle {
                radius: 6
                color: control.activeFocus ? "#FFFFFF" : "#F3F4F6"
                border.color: control.activeFocus ? "#3B82F6" : "transparent"
                border.width: 1
                Behavior on color { ColorAnimation { duration: 150 } }
                Behavior on border.color { ColorAnimation { duration: 150 } }
            }
        }

        // 1. 用户名
        ModernTextField { id: username; placeholderText: "用户名" }

        // 2. 邮箱
        ModernTextField { id: email; placeholderText: "邮箱"; inputMethodHints: Qt.ImhEmailCharactersOnly }

        // 3. 密码
        ModernTextField { id: pwd; placeholderText: "密码"; echoMode: TextInput.Password }

        // 4. 确认密码
        ModernTextField { id: confirmPwd; placeholderText: "确认密码"; echoMode: TextInput.Password }

        // 5. 验证码 + 获取按钮
        RowLayout {
            Layout.preferredWidth: 260
            spacing: 10

            ModernTextField {
                id: verifyCode
                placeholderText: "验证码"
                Layout.fillWidth: true
                Layout.preferredWidth: -1 // 覆盖默认宽度，让它自适应
            }

            Button {
                id: getCodeBtn
                text: "获取验证码"
                Layout.preferredWidth: 90
                Layout.preferredHeight: 25
                enabled: !countdownTimer.running

                background: Rectangle {
                    radius: 6
                    color: getCodeBtn.enabled ? (getCodeBtn.pressed ? "#2563EB" : (getCodeBtn.hovered ? "#60A5FA" : "#3B82F6")) : "#D1D5DB"
                    Behavior on color { ColorAnimation { duration: 150 } }
                }
                contentItem: Text {
                    text: getCodeBtn.text
                    color: "white"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    console.log("向邮箱发送验证码：" + email.text)
                    countdownTimer.start()
                }
            }
        }

        // 错误提示文字
        Text {
            id: errorText
            text: "两次输入的密码不一致"
            color: "#EF4444"
            font.pixelSize: 12
            visible: false
            Layout.alignment: Qt.AlignHCenter
        }

        // 注册按钮
        Button {
            id: regBtn
            text: "注册"
            Layout.preferredWidth: 260
            Layout.preferredHeight: 40
            Layout.topMargin: 10

            background: Rectangle {
                radius: 6
                gradient: Gradient {
                    GradientStop { position: 0.0; color: regBtn.pressed ? "#2563EB" : (regBtn.hovered ? "#60A5FA" : "#3B82F6") }
                    GradientStop { position: 1.0; color: regBtn.pressed ? "#1D4ED8" : (regBtn.hovered ? "#3B82F6" : "#2563EB") }
                }
                Behavior on color { ColorAnimation { duration: 150 } }
            }
            contentItem: Text {
                text: regBtn.text
                color: "white"
                font.bold: true
                font.pixelSize: 15
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: {
                if (username.text === "" || email.text === "" || pwd.text === "" || verifyCode.text === "") {
                    errorText.text = "请填写所有必填项"
                    errorText.visible = true
                    return
                }
                if (pwd.text !== confirmPwd.text) {
                    errorText.text = "两次输入的密码不一致"
                    errorText.visible = true
                    return
                }
                errorText.visible = false
                console.log("注册提交：", username.text, email.text, pwd.text, verifyCode.text)
            }
        }

        // 返回登录按钮
        Button {
            id: toLoginBtn
            text: "已有账号？去登录"
            flat: true
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 5

            contentItem: Text {
                text: toLoginBtn.text
                color: toLoginBtn.hovered ? "#3B82F6" : "#6B7280"
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Item {}
            onClicked: switchLogin()
        }
    }

    // 验证码倒计时定时器（原封不动）
    Timer {
        id: countdownTimer
        interval: 1000
        repeat: true
        property int secondsLeft: 60

        onTriggered: {
            secondsLeft--
            if (secondsLeft <= 0) {
                stop()
                getCodeBtn.text = "获取验证码"
                secondsLeft = 60
            } else {
                getCodeBtn.text = "重新获取(" + secondsLeft + "s)"
            }
        }

        onRunningChanged: {
            if (running) {
                secondsLeft = 60
                getCodeBtn.text = "重新获取(" + secondsLeft + "s)"
            }
        }
    }
}
