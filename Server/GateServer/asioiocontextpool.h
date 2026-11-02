#pragma once
#include <vector>
#include <boost/asio.hpp>
#include "Singleton.h"
#include "const.h"
#include <thread>
#include <memory>
class AsioIOContextPool:public Singleton<AsioIOContextPool>
{
    friend Singleton<AsioIOContextPool>;
public:
    using IOContext = net::io_context;
    //防止在iocontext.run()的时候没有任务退出
    using WorkGuard = net::executor_work_guard<IOContext::executor_type>;
    using WorkGuardPtr = std::unique_ptr<WorkGuard>;

    ~AsioIOContextPool();

    AsioIOContextPool(const AsioIOContextPool &) = delete;
    AsioIOContextPool &operator=(const AsioIOContextPool &) = delete;
    //利用round_robin的方式返回一个io_service,就是按顺序轮流返回
    boost::asio::io_context &GetIoService();
    void Stop(); //服务池停止的时候调用回收一些资源,并且通知挂起的线程唤醒

private:
    AsioIOContextPool(std::size_t size = 2);//也可以用std::thread::hardware_concurrency()来获取
    std::vector<IOContext> _ioservices;
    std::vector<WorkGuardPtr> _works; //防止ioservices退出的,两者数量相等
    std::vector<std::thread> _threads; //每一个ioservice跑在每一个线程
    std::size_t _nextIOContext;//返回下一个ioservice的索引
};
