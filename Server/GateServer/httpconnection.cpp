#include "httpconnection.h"
#include <cctype>
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
        } catch (const std::exception& exp) {
            std::cout << "exception error is" << exp.what() << std::endl;
        }
    });
}

void HttpConnection::CheckDeadLine()
{
    auto self = shared_from_this();
    // 从现在开始重新计时，而不是沿用连接创建时的时间点
    deadline_.expires_after(std::chrono::seconds(60));
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
        PreParseGetParam();
        bool success = LogicSystem::GetInstance()->HandleGet(_get_url, shared_from_this());
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

    if (_request.method() == http::verb::post) {
        bool success = LogicSystem::GetInstance()->HandlePost(_request.target(), shared_from_this());
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

unsigned char ToHex(unsigned char x)
{
    //转化为十六进制
    return x > 9 ? x + 55 : x + 48;
}

bool FromHex(unsigned char x, unsigned char& value)
{
    //从十六进制转换为十进制，非法字符返回 false
    if (x >= 'A' && x <= 'F') {
        value = x - 'A' + 10;
    } else if (x >= 'a' && x <= 'f') {
        value = x - 'a' + 10;
    } else if (x >= '0' && x <= '9') {
        value = x - '0';
    } else {
        return false;
    }
    return true;
}

std::string UrlEncode(const std::string& str)
{
    //编码
    //把字符串里那些“在 URL 里会引发歧义的字符”，翻译成浏览器和服务器都能安全识别的格式（%XX）。
    std::string strTemp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; i++)
    {
        // 判断是否仅有数字和字母构成，以及 URL 规范中允许的 4 个特殊字符
        if (isalnum((unsigned char)str[i]) ||
            (str[i] == '-') ||
            (str[i] == '_') ||
            (str[i] == '.') ||
            (str[i] == '~'))
        {
            strTemp += str[i];
        }
        else if (str[i] == ' ') // 为空格字符
        {
            strTemp += "+";
        }
        else
        {
            // 其他字符需要提前加 % 并且高四位和低四位分别转为 16 进制
            strTemp += '%';
            strTemp += ToHex((unsigned char)str[i] >> 4);    // 取高 4 位
            strTemp += ToHex((unsigned char)str[i] & 0x0F);  // 取低 4 位
        }
    }
    return strTemp;
}

std::string UrlDecode(const std::string& str, bool plus_as_space = true)
{
    std::string strTemp;
    strTemp.reserve(str.size());
    size_t length = str.length();
    for (size_t i = 0; i < length; i++)
    {
        // 查询字符串中 + 表示空格；路径中 + 是普通字符
        if (plus_as_space && str[i] == '+') {
            strTemp += ' ';
        }
        // 遇到 % 将后面的两个字符从 16 进制转为 char 再拼接
        else if (str[i] == '%')
        {
            unsigned char high = 0;
            unsigned char low = 0;
            // 转义不完整或含非法十六进制字符时，原样保留 '%'
            if (i + 2 >= length ||
                !FromHex(static_cast<unsigned char>(str[i + 1]), high) ||
                !FromHex(static_cast<unsigned char>(str[i + 2]), low)) {
                strTemp += str[i];
                continue;
            }
            strTemp += static_cast<char>(high * 16 + low);
            i += 2;
        }
        else {
            strTemp += str[i];
        }
    }
    return strTemp;
}

void HttpConnection::PreParseGetParam() {
    //解析 HTTP GET 请求中的 URL，将基础路径和查询参数（Query Parameters）分离开来，并提取出具体的键值对。
    std::string uri = _request.target();
    _get_params.clear();

    // 查找查询字符串的开始位置 (即 '?' 的位置)
    auto query_pos = uri.find('?');
    if (query_pos == std::string::npos) {
        _get_url = UrlDecode(uri, false);
        return;
    }

    // 路径中的 '+' 是普通字符，不能当作空格
    _get_url = UrlDecode(uri.substr(0, query_pos), false);
    std::string query_string = uri.substr(query_pos + 1);

    // 用下标扫描，避免反复 erase 造成 O(n^2)
    size_t start = 0;
    while (start <= query_string.size()) {
        size_t end = query_string.find('&', start);
        if (end == std::string::npos) {
            end = query_string.size();
        }

        auto pair = query_string.substr(start, end - start);
        if (!pair.empty()) {
            size_t eq_pos = pair.find('=');
            if (eq_pos != std::string::npos) {
                _get_params[UrlDecode(pair.substr(0, eq_pos))] = UrlDecode(pair.substr(eq_pos + 1));
            } else {
                // 没有 '=' 的参数保留 key，值为空
                _get_params[UrlDecode(pair)] = "";
            }
        }

        if (end == query_string.size()) {
            break;
        }
        start = end + 1;
    }
}
