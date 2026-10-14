import QtQuick
import QtQuick.Controls

ApplicationWindow {
    width: 350; height: 500
    visible: true
    title: "OpenChat"

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: loginComponent // 用 Component 加载初始页面
    }

    // 预加载登录页组件
    Component {
        id: loginComponent
        LoginPage {
            // 登录页收到"去注册"的请求时，切换到注册页
            onSwitchRegister: stack.push(registerComponent)
        }
    }

    // 预加载注册页组件
    Component {
        id: registerComponent
        RegisterPage {
            // 注册页收到"去登录"的请求时，返回上一页
            onSwitchLogin: stack.pop()
        }
    }
}
