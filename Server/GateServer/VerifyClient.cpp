#include "VerifyClient.h"

#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <string>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <json/json.h>

#include "ConfigMgr.h"
#include "const.h"

//把一段 JSON 数据通过 HTTP 发出去，把回复接回来。
namespace {

// HTTP 响应结果。
// 这里不直接返回 Beast 的 response 对象，是为了把网络细节限制在本文件内。
struct HttpResponse {
    int status = 0;
    std::string body;
    std::string error;

    bool ok() const {
        return status == 200 && error.empty();
    }
};

// 同步发送一个 JSON POST 请求。
//
// 对应 gRPC 里 grpc::Channel + stub 的底层能力：
// - DNS 解析
// - TCP 连接
// - HTTP 请求/响应收发
// - 超时控制
//
// 使用短连接（Connection: close），逻辑简单；
// 如果后续验证码或登录接口压力变大，再考虑连接池或异步客户端。
HttpResponse PostJson(const std::string& host,
                      const std::string& port,
                      const std::string& target,
                      const std::string& body,
                      std::chrono::milliseconds timeout) {
    HttpResponse result;

    try {
        net::io_context ioc;
        tcp::resolver resolver(ioc);
        beast::tcp_stream stream(ioc);

        // 连接超时。
        // expires_after 对 connect/write/read 都生效，避免下游服务挂掉时 GateServer 卡住。
        const auto endpoints = resolver.resolve(host, port);
        stream.expires_after(timeout);//强行掐断连接并抛出超时错误，防止程序卡死。
        stream.connect(endpoints);

        http::request<http::string_body> request{http::verb::post, target, 11};
        request.set(http::field::host, host + ":" + port);
        request.set(http::field::content_type, "application/json");
        request.set(http::field::accept, "application/json");
        request.set(http::field::connection, "close");
        request.body() = body;
        request.prepare_payload();//添加content——length,不然node.js会一直等待长度

        // 写请求超时。
        stream.expires_after(timeout);// 给 write 上发条
        http::write(stream, request);

        // 读响应超时。
        beast::flat_buffer buffer;// 给 write 上发条
        http::response<http::string_body> response;
        stream.expires_after(timeout);
        http::read(stream, buffer, response);

        result.status = static_cast<int>(response.result_int());
        result.body = response.body();

        if (response.result() != http::status::ok) {
            result.error = "http status " + std::to_string(result.status);
        }
    } catch (const std::exception& e) {
        // 连接失败、超时、协议错误等统一转成 error 字符串，不向调用方抛异常。
        result.error = e.what();
    }

    return result;
}

}  // namespace

VerifyRsp VerifyClient::GetVerifyCode(const std::string& email) {
    VerifyRsp rsp;
    rsp.email = email;
    rsp.error = ErrorCodes::RPCFailed;//先做好最坏的打算

    // 从 config.ini 读取 VerifyServer 地址。
    // 注意：配置里的 section 名是 VarifyServer，避免和客户端请求路径不一致。
    const auto& cfg = ConfigMgr::Inst();
    const auto& host = cfg["VarifyServer"]["Host"];
    const auto& port = cfg["VarifyServer"]["Port"];
    const auto& timeout_str = cfg["VarifyServer"]["TimeoutMs"];

    if (host.empty() || port.empty()) {
        std::cerr << "VerifyServer host/port is missing in config.ini" << std::endl;
        return rsp;
    }

    // 默认 5 秒。SMTP 发信可能比普通接口慢，因此超时不宜设得太短。
    int timeout_ms = 5000;
    if (!timeout_str.empty()) {
        try {
            timeout_ms = std::stoi(timeout_str);
        } catch (const std::exception&) {
            std::cerr << "VerifyServer TimeoutMs is invalid, use 5000ms" << std::endl;
        }
    }
    if (timeout_ms <= 0) {
        timeout_ms = 5000;
    }

    // 构造 HTTP 请求体：{"email":"xxx@qq.com"}
    Json::Value request;
    request["email"] = email;
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    const std::string body = Json::writeString(writer, request);

    // 对应gRPC 的 stub->GetVarifyCode(...)
    const auto response = PostJson(host, port, "/get_varifycode", body,
                                   std::chrono::milliseconds(timeout_ms));
    if (!response.ok()) {
        std::cerr << "VerifyServer request failed: " << response.error << std::endl;
        return rsp;
    }

    // 解析响应：{"error":0,"email":"xxx@qq.com"}
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    std::string errs;
    if (!reader->parse(response.body.data(),
                       response.body.data() + response.body.size(),
                       &root, &errs)) {
        std::cerr << "VerifyServer response parse failed: " << errs << std::endl;
        return rsp;
    }

    rsp.error = root.get("error", Json::Value(ErrorCodes::RPCFailed)).asInt();
    if (root.isMember("email")) {
        rsp.email = root["email"].asString();
    }
    if (root.isMember("code")) {
        rsp.code = root["code"].asString();
    }

    return rsp;
}
