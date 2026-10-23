#pragma once

#include <string>
#include "Singleton.h"
//该类后续用于gateserver给varifyserver传话

//通过 HTTP + JSON 调用 Node 验证码服务。
struct VerifyRsp {
    int error = 0;
    std::string email;
    // 只返回 error/email，验证码通过邮件发送。
    // code 字段是为了兼容 proto 的结构，但 GateServer 不应该把它返回给客户端。
    std::string code;
};

class VerifyClient : public Singleton<VerifyClient> {
    friend class Singleton<VerifyClient>;

public:
    // 请求 VerifyServer 发送验证码。
    // 成功时返回 error=0；网络失败、超时、响应解析失败时返回 ErrorCodes::RPCFailed。
    VerifyRsp GetVerifyCode(const std::string& email);

private:
    VerifyClient() = default;
};
