#include "VerifyClient.h"

#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <string>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <json/json.h>

#include "ConfigMgr.h"
#include "const.h"
#include "httpconpool.h"

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

// 从连接池借一条连接，同步发送一个 JSON POST 请求，然后把连接归还。
//
// 与旧实现的区别：
// 1. 不再每次新建 io_context/tcp_stream，而是复用 HttpConPool 里的连接；
// 2. 使用 keep-alive，请求结束后连接会放回池中；
// 3. 连接的超时已经由 HttpConPool 在建立连接时设置好，这里不再调用 expires_after。
//
// 重试说明：
// - 池里的 keep-alive 连接可能已经被服务端关闭（例如 VerifyServer 重启）；
// - 这种情况下 write/read 会抛异常，我们会把连接标记为不可复用并换一条新连接重试；
// - maxRetries 由 config.ini 的 MaxRetries 控制，默认 1。
HttpResponse PostJson(HttpConPool& pool,
                      const std::string& host,
                      const std::string& port,
                      const std::string& target,
                      const std::string& body,
                      int maxRetries) {
    HttpResponse result;

    // 最多尝试 maxRetries + 1 次。
    for (int attempt = 0; attempt <= maxRetries; ++attempt) {
        // 每一轮开始前清空上一次尝试留下的结果。
        // 否则上一轮的 error 会残留，导致重试成功仍然被当成失败。
        result = HttpResponse{};

        // 从池里借一条独占连接。
        // 如果池已满，这里会阻塞等待，起到限制下游并发的作用。
        auto conn = pool.getConnection();
        if (!conn) {
            result.error = "no available connection";
            return result;
        }

        try {
            http::request<http::string_body> request{http::verb::post, target, 11};
            request.set(http::field::host, host + ":" + port);
            request.set(http::field::content_type, "application/json");
            request.set(http::field::accept, "application/json");
            // 使用 keep-alive，服务端响应结束后连接仍然可用，可以放回池里复用。
            request.set(http::field::connection, "keep-alive");
            request.body() = body;
            request.prepare_payload();//添加content——length,不然node.js会一直等待长度

            // conn->stream 的 SO_RCVTIMEO/SO_SNDTIMEO 已由 HttpConPool 设置，
            // 因此同步 write/read 在超时后会抛异常，不会无限阻塞。
            http::write(conn->stream, request);

            beast::flat_buffer buffer;
            http::response<http::string_body> response;
            http::read(conn->stream, buffer, response);

            result.status = static_cast<int>(response.result_int());
            result.body = response.body();

            if (response.result() != http::status::ok) {
                result.error = "http status " + std::to_string(result.status);
            }

            // 只有服务端也同意 keep-alive 时，这条连接才值得复用。
            // 如果响应头是 Connection: close，就标记为不可复用，归还时直接销毁。
            conn->reusable = response.keep_alive();
            pool.returnConnection(std::move(conn));
            return result;
        } catch (const std::exception& e) {
            // 网络错误、超时、协议错误等：这条连接已经不可信，标记后归还销毁。
            result.error = e.what();
            conn->reusable = false;
            pool.returnConnection(std::move(conn));

            if (attempt == maxRetries) {
                break;
            }

            std::cerr << "VerifyServer request failed, retrying ("
                      << (attempt + 1) << "/" << maxRetries << "): "
                      << result.error << std::endl;
        }
    }

    return result;
}

}  // namespace

// 构造函数：从配置读取 VerifyServer 地址、连接池大小和超时，然后创建连接池。
VerifyClient::VerifyClient() {
    const auto& cfg = ConfigMgr::Inst();

    const std::string host = cfg["VarifyServer"]["Host"];
    const std::string port = cfg["VarifyServer"]["Port"];

    // PoolSize：连接池最多保留多少条到 VerifyServer 的连接。
    // 缺失或非法时使用 5。
    std::size_t poolSize = 5;
    const auto& poolSizeStr = cfg["VarifyServer"]["PoolSize"];
    if (!poolSizeStr.empty()) {
        try {
            poolSize = std::stoul(poolSizeStr);
        } catch (const std::exception&) {
            std::cerr << "VarifyServer PoolSize is invalid, use 5" << std::endl;
        }
    }
    if (poolSize == 0) {
        poolSize = 5;
    }

    // TimeoutMs：连接阶段的超时，以及单次同步读写的 socket 超时。
    // 默认 5 秒。SMTP 发信可能比普通接口慢，因此超时不宜设得太短。
    int timeoutMs = 5000;
    const auto& timeoutStr = cfg["VarifyServer"]["TimeoutMs"];
    if (!timeoutStr.empty()) {
        try {
            timeoutMs = std::stoi(timeoutStr);
        } catch (const std::exception&) {
            std::cerr << "VarifyServer TimeoutMs is invalid, use 5000ms" << std::endl;
        }
    }
    if (timeoutMs <= 0) {
        timeoutMs = 5000;
    }

    _pool = std::make_unique<HttpConPool>(
        poolSize, host, port, std::chrono::milliseconds(timeoutMs));
}

VerifyClient::~VerifyClient() = default;

VerifyRsp VerifyClient::GetVerifyCode(const std::string& email) {
    VerifyRsp rsp;
    rsp.email = email;
    rsp.error = ErrorCodes::RPCFailed;//先做好最坏的打算

    // 从 config.ini 读取 VerifyServer 地址。
    // 注意：配置里的 section 名是 VarifyServer，避免和客户端请求路径不一致。
    const auto& cfg = ConfigMgr::Inst();
    const auto& host = cfg["VarifyServer"]["Host"];
    const auto& port = cfg["VarifyServer"]["Port"];

    if (host.empty() || port.empty()) {
        std::cerr << "VerifyServer host/port is missing in config.ini" << std::endl;
        return rsp;
    }

    // MaxRetries：连接被服务端关闭（例如 VerifyServer 重启）时最多重试几次。
    // 默认重试 1 次。
    int maxRetries = 1;
    const auto& retriesStr = cfg["VarifyServer"]["MaxRetries"];
    if (!retriesStr.empty()) {
        try {
            maxRetries = std::stoi(retriesStr);
        } catch (const std::exception&) {
            std::cerr << "VarifyServer MaxRetries is invalid, use 1" << std::endl;
        }
    }
    if (maxRetries < 0) {
        maxRetries = 0;
    }

    // 构造 HTTP 请求体：{"email":"xxx@qq.com"}
    Json::Value request;
    request["email"] = email;
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    const std::string body = Json::writeString(writer, request);

    // 从连接池借连接，发送请求。
    const auto response = PostJson(*_pool, host, port, "/get_varifycode", body, maxRetries);
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
