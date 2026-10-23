#include <iostream>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include <json/reader.h>
#include <stdexcept>
#include "cserver.h"
#include "ConfigMgr.h"
int main()
{
    try {
        // At() 会在 section/key 缺失时抛异常，避免配置拼写错误被静默忽略。
        const auto& port_str = ConfigMgr::Inst().At("GateServer").At("Port");
        if (port_str.empty()) {
            throw std::runtime_error("GateServer.Port is empty in config.ini");
        }

        int port_value = 0;
        try {
            port_value = std::stoi(port_str);
        } catch (const std::exception&) {
            throw std::runtime_error("GateServer.Port is not a valid integer in config.ini");
        }

        if (port_value <= 0 || port_value > 65535) {
            throw std::runtime_error("GateServer.Port is invalid in config.ini");
        }
        unsigned short port = static_cast<unsigned short>(port_value);
        net::io_context ioc{1};
        boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code error, int signal_numbers) {
            if (error) { return; }
            ioc.stop();
        });
        std::make_shared<Cserver>(ioc, port)->Start();
        std::cout << "GateServer listen in port " << port << std::endl;
        ioc.run();
    } catch (std::exception const &ec) {
        std::cout << "the exception is " << ec.what() << std::endl;
        return EXIT_FAILURE;//返回出错的码
    }
}
