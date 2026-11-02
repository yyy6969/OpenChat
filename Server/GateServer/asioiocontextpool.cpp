#include "asioiocontextpool.h"
#include <iostream>

AsioIOContextPool::AsioIOContextPool(std::size_t size)
    : _ioservices(size)
    , _works(size)
    , _nextIOContext(0)
{
    for (int i = 0; i < size; i++) {
        _works[i] = std::unique_ptr<WorkGuard>(new WorkGuard(_ioservices[i].get_executor()));
    }

    //遍历多个ioservice，创建多个线程，在每个线程里面跑一个ioservice
    for (int i = 0; i < _ioservices.size(); i++) {
        _threads.emplace_back([this, i]() { _ioservices[i].run(); });
    }
}

AsioIOContextPool::~AsioIOContextPool()
{
    Stop();
    std::cout << "AsioIoContextPool destruct" << std::endl;
}

boost::asio::io_context &AsioIOContextPool::GetIoService()
{
    auto& service = _ioservices[_nextIOContext++]; //轮询取出
    if (_nextIOContext == _ioservices.size()) { _nextIOContext = 0; }
    return service;
}


void AsioIOContextPool::Stop()
{
    //在析构函数之前用
    //因为仅仅执行work.reset()不能让iocontext从run的状态中退出
    //当iocontext已经绑定了读或者写的监听事件后，还需要手动stop()
    for (std::size_t i = 0; i < _works.size(); ++i) {
        _ioservices[i].stop();
        _works[i].reset();
    }

    for (auto &t : _threads) {
        t.join();
    }
}