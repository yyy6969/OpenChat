#pragma once

#include <memory>
#include <string>
#include "Singleton.h"
//该类后续用于gateserver给varifyserver传话

// HttpConPool 定义在 httpconpool.h 中。
// 这里只做前置声明，避免 VerifyClient.h 的使用者被动引入 Beast/Asio 头文件。
class HttpConPool;

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
    // _pool 的类型是 unique_ptr<HttpConPool>，而 HttpConPool 在这里是不完整类型。
    // 不完整类型无法在头文件里生成 unique_ptr 的默认析构代码，
    // 所以析构函数必须在 .cpp 中定义（那里能看到完整的 HttpConPool）。
    ~VerifyClient();

    // 请求 VerifyServer 发送验证码。
    // 成功时返回 error=0；网络失败、超时、响应解析失败时返回 ErrorCodes::RPCFailed。
    VerifyRsp GetVerifyCode(const std::string& email);

private:
    VerifyClient();

    // 到 VerifyServer 的 HTTP 连接池。
    // 每次 GetVerifyCode 从池里借一条连接，用完归还，避免重复 TCP 握手。
    std::unique_ptr<HttpConPool> _pool;
};
