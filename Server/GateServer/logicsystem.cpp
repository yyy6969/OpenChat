#include "logicsystem.h"
#include "httpconnection.h"
LogicSystem::~LogicSystem() {}

void LogicSystem::RegGet(std::string url, HttpHandler handler)
{
    _get_handlers.insert(std::make_pair(url, handler));
}

LogicSystem::LogicSystem()
{
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
    if (_get_handlers.find(path) == _get_handlers.end()) { return false; }

    _get_handlers[path](con);
    return true;
}
