#include "cserver.h"
#include "httpconnection.h"
Cserver::Cserver(boost::asio::io_context &ioc, unsigned short port)
    : _ioc(ioc)
    , _acceptor(ioc, tcp::endpoint(tcp::v4(), port))
    , _socket(ioc)
{}

void Cserver::Start()
{
    auto self = shared_from_this();
    _acceptor.async_accept(_socket, [self](beast::error_code ec) {
        //self而不是this,可以使这个self指针加一，保证这个异步接受中self不会别释放
        try {
            //如果出粗就放弃连接
            if (ec) {
                self->Start();
                return;
            }
            //创建新连接，并且创建HttpConnection来管理这个连接
            std::make_shared<HttpConnection>(std::move(self->_socket))->Start();
            //继续监听
            self->Start();
        } catch (std::exception& exp) {
            std::cout << "exception is " << exp.what() << std::endl;
            self->Start();
        }
    });
}
