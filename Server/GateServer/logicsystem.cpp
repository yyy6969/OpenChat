#include "logicsystem.h"
#include <iostream>
#include "httpconnection.h"
LogicSystem::~LogicSystem() {}

void LogicSystem::RegGet(std::string url, HttpHandler handler)
{
    //注册路由
    //结构化绑定
    auto [it, inserted] = _get_handlers.insert(std::make_pair(url, std::move(handler)));
    if (!inserted) {
        std::cerr << "duplicate route registered: " << url << std::endl;
    }
}

void LogicSystem::RegPost(std::string url, HttpHandler handler)
{
    // 注册路由
    // 结构化绑定
    auto [it, inserted] = _post_handlers.insert(std::make_pair(url, std::move(handler)));
    if (!inserted) {
        std::cerr << "duplicate route registered: " << url << std::endl;
    }
}

LogicSystem::LogicSystem()
{
    //初始化默认路由
    RegGet("/get_test", [](std::shared_ptr<HttpConnection> connection) {
        beast::ostream(connection->_response.body())<<"receive get_test req"<<std::endl;
        int i = 0;
        for (auto& elem : connection->_get_params) {
            beast::ostream(connection->_response.body()) << "param " << i << " key is " << elem.first;
            beast::ostream(connection->_response.body()) << "param " << i << " value is " << elem.second<<std::endl;
            ++i;
        }
    });
    RegGet("/", [](std::shared_ptr<HttpConnection> connection) {
        connection->_response.set(http::field::content_type, "text/html");
        beast::ostream(connection->_response.body())<<"<h1>GateServer is running</h1>";
    });

    RegPost("/get_varifycode", [](std::shared_ptr<HttpConnection> connection) {
        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
        std::cout << "receive body is " << body_str << std::endl;
        connection->_response.set(http::field::content_type, "text/json");
        Json::Value root;
        Json::Value src_root; //来源
        Json::CharReaderBuilder builder;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        std::string errs;
        bool parse_success = reader->parse(body_str.data(), body_str.data() + body_str.size(), &src_root, &errs);
        if (!parse_success) {
            std::cout << "Failed to parse JSON data: " << errs << std::endl;
            root["error"] = ErrorCodes::Error_Json;
            std::string jsonstr = root.toStyledString(); //序列化
            beast::ostream(connection->_response.body()) << jsonstr;
            return;
        }

        if (!src_root.isMember("email")) {
            std::cout << "Failed to parse JSON data!" << std::endl;
            root["error"] = ErrorCodes::Error_Json;
            std::string jsonstr = root.toStyledString(); //序列化
            beast::ostream(connection->_response.body()) << jsonstr;
            return;
        }

        auto email = src_root["email"].asString();
        std::cout << "email is " << email << std::endl;
        root["error"] = 0;
        root["email"] = src_root["email"];
        std::string jsonstr = root.toStyledString();
        beast::ostream(connection->_response.body()) << jsonstr;
    });
}

bool LogicSystem::HandleGet(const std::string& path, std::shared_ptr<HttpConnection> con)
{
    auto it = _get_handlers.find(path);
    if (it == _get_handlers.end()) {
        return false;
    }
    it->second(con);   // 直接用迭代器拿到 handler，不再二次查找
    return true;
}

bool LogicSystem::HandlePost(std::string path, std::shared_ptr<HttpConnection> con) {
    auto it = _post_handlers.find(path);
    if (it == _post_handlers.end()) {
        return false;
    }
    it->second(con);   // 直接用迭代器拿到 handler，不再二次查找
    return true;
}