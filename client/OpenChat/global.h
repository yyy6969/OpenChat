#ifndef GLOBAL_H
#define GLOBAL_H
#include<QByteArray>
#include<QMetaType>
#include<QString>

// 服务端地址由 config.ini 读取后写入，接口路径统一放这里
extern QString gate_url_prefix;
inline constexpr char kGetVerifyCodePath[] = "/get_varifycode";
inline constexpr char kUserRegisterPath[] = "/user_register";
inline constexpr char kUserLoginPath[] = "/login";

enum ReqId{
    ID_GET_VERIFY_CODE = 1001,//获取验证码
    ID_REG_USER = 1002,//注册用户
    ID_LOGIN_USER = 1003,//登录
};

enum Modules{
    REGISTERMOD = 0,
    LOGINMOD = 1
};

enum ErrorCode{
    SUCCESS = 0,
    ERR_JSON = 1,//JSOn解析失败
    ERR_NETWORK = 2
};

Q_DECLARE_METATYPE(ReqId)
Q_DECLARE_METATYPE(Modules)
Q_DECLARE_METATYPE(ErrorCode)

#endif // GLOBAL_H
