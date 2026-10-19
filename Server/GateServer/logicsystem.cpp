#include "logicsystem.h"
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

LogicSystem::LogicSystem()
{
    //初始化默认路由
    RegGet("/get_test", [](std::shared_ptr<HttpConnection> connection) {
        beast::ostream(connection->_response.body())<<"receive get_test req";
    });
    RegGet("/", [](std::shared_ptr<HttpConnection> connection) {
        connection->_response.set(http::field::content_type, "text/html");
        beast::ostream(connection->_response.body())<<"<h1>GateServer is running</h1>";
    });
}

bool LogicSystem::HandleGet(std::string path, std::shared_ptr<HttpConnection> con)
{
    auto it = _get_handlers.find(path);
    if (it == _get_handlers.end()) {
        return false;
    }
    it->second(con);   // 直接用迭代器拿到 handler，不再二次查找
    return true;
}
