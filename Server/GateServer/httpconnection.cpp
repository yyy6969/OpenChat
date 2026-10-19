#include "httpconnection.h"
#include <iostream>
#include "logicsystem.h"
HttpConnection::HttpConnection(tcp::socket socket)
    : _socket(std::move(socket))
{}

void HttpConnection::Start()
{
    auto self = shared_from_this();
    http::async_read(_socket, _buffer, _request, [self](boost::system::error_code err, std::size_t bytes_transferred) {
        try {
            if (err) {
                std::cout << "http read error is " << err.what() << std::endl;
                return;
            }
            boost::ignore_unused(bytes_transferred); //http忽略粘包处理
            self->HandleReq();
            self->CheckDeadLine();
        } catch (std::exception exp) {
            std::cout << "exception error is" << exp.what() << std::endl;
        }
    });
}

void HttpConnection::CheckDeadLine()
{
    auto self = shared_from_this();
    deadline_.async_wait([self](beast::error_code ec) {
        //ec==0为true
        if(!ec)
        {
            self->_socket.close();
        }
    });
}

void HttpConnection::WriteResponse()
{
    auto self = shared_from_this();
    _response.content_length(_response.body().size());

    http::async_write(_socket, _response,[self](boost::system::error_code err, std::size_t bytes_transferred)
    {
        if (err) {
            if (err != boost::asio::error::operation_aborted)
                {
                    std::cerr << "async_write failed: " << err.message()
                    << ", bytes_transferred: " << bytes_transferred << std::endl;
                    }
                        boost::system::error_code ignore_ec;
                        self->_socket.close(ignore_ec);   // 兜底关闭，防止 socket 滞留
                        self->deadline_.cancel();
                        return;                            // 失败路径到此为止
                        }

                    // 成功路径：优雅半关闭
                    boost::system::error_code shutdown_ec;
                    self->_socket.shutdown(tcp::socket::shutdown_send, shutdown_ec);
                    if (shutdown_ec && shutdown_ec != boost::asio::error::not_connected)
                    {
                    std::cerr << "socket shutdown failed: " << shutdown_ec.message() <<std::endl;
                }

    self->deadline_.cancel();
        });
}

void HttpConnection::HandleReq()
{
    //设置版本
    _response.version(_request.version());
    _response.keep_alive(false); //不保活
    if (_request.method() == http::verb::get) {
        bool success = LogicSystem::GetInstance()->HandleGet(_request.target(), shared_from_this());
        if (!success) {
            _response.result(http::status::not_found);
            _response.set(http::field::content_type, "text/plain");
            beast::ostream(_response.body()) << "url not found"<<"\r\n";
            WriteResponse();
            return;
        }
        //成功了
        _response.result(http::status::ok);
        _response.set(http::field::server, "GateServer");
        WriteResponse();
        return;
    }
}
