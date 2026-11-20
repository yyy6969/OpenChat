#include "cserver.h"
#include <chrono>
#include <iostream>
#include "httpconnection.h"
#include "asioiocontextpool.h"
Cserver::Cserver(boost::asio::io_context &ioc, unsigned short port)
    : _ioc(ioc)
    , _acceptor(ioc, tcp::endpoint(tcp::v4(), port))
    //, _socket(ioc)
    , _retry_timer(ioc)
{}

void Cserver::Start()
{
    auto self = shared_from_this();
    //从线程池中取iocontext
    auto& io_context = AsioIOContextPool::GetInstance()->GetIoService();
    std::shared_ptr<HttpConnection> new_con = std::make_shared<HttpConnection>(io_context);
    _acceptor.async_accept(new_con->GetSocket(), [new_con,self](beast::error_code ec) {
        //self而不是this,可以使这个self指针加一，保证这个异步接受中self不会别释放（保活）
        //acceptor 已关闭或 io_context 正在停止，直接退出，避免忙循环
        if (ec == net::error::operation_aborted) {
            return;
        }
        if (ec) {
            std::cerr << "accept error: " << ec.message() << std::endl;
            //短暂退避后再重试，避免 fd 耗尽等错误导致死循环
            self->_retry_timer.expires_after(std::chrono::seconds(1));
            self->_retry_timer.async_wait([self](beast::error_code timer_ec) {
                if (!timer_ec) {
                    self->Start();
                }
            });
            return;
        }
        try {
            //现在不需要在这里创建了，直接用new——con连接
            new_con->Start();
        } catch (const std::exception& exp) {
            std::cerr << "exception is " << exp.what() << std::endl;
        }
        //继续监听
        self->Start();
    });
}
