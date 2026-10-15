#ifndef GLOBAL_H
#define GLOBAL_H
#include<QByteArray>
enum ReqId{
    ID_GET_VERIFY_CODE = 1001,//获取验证码
    ID_REG_USER = 1002,//注册用户
};

enum Modules{
    REGISTERMOD = 0
};

enum ErrorCode{
    SUCCESS = 0,
    ERR_JSON = 1,//JSOn解析失败
    ERR_NETWORK = 2
};
#endif // GLOBAL_H
