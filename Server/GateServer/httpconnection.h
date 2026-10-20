#pragma once
#include "const.h"
class HttpConnection:public std::enable_shared_from_this<HttpConnection>
{
    friend class LogicSystem;
public:
    HttpConnection(tcp::socket socket);
    void Start(); //监听读写
private:
    void CheckDeadLine();
    void WriteResponse();
    void HandleReq(); //处理请求,例如解析请求头
    void PreParseGetParam();
    tcp::socket _socket;
    beast::flat_buffer _buffer{8192}; //8192 就是 8KB
    http::request<http::dynamic_body> _request;
    http::response<http::dynamic_body> _response;
    net::steady_timer deadline_{
        _socket.get_executor(), std::chrono::seconds(60)//用来做定时器判断请求是否超时
    };
    std::string _get_url;
    std::unordered_map<std::string, std::string> _get_params;
};
