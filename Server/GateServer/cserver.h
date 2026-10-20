#pragma once
#include "const.h"
//
class Cserver:public std::enable_shared_from_this<Cserver>
{
public:
    Cserver(net::io_context &ioc, unsigned short port);
    void Start();

private:
    net::io_context& _ioc;//ioc是不存在拷贝构造的
    tcp::acceptor _acceptor;
    tcp::socket _socket;//进行服用
    net::steady_timer _retry_timer; //accept 出错后的重试定时器
};
