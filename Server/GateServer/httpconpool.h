#pragma once

#include "const.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

// 一条可复用的 HTTP 连接。
//
// 设计说明：
// 1. 每条连接拥有自己的 io_context，避免多个连接共用同一个 executor 互相干扰。
// 2. stream 用 ioc 构造，生命周期由 PooledHttpConn 管理。
// 3. 该对象同一时刻只允许一个线程使用；互斥由 HttpConPool 的 get/return 保证。
// 4. reusable 标记连接是否还能继续复用：
//    - 正常读完响应且服务端允许 keep-alive -> true
//    - 读写异常、超时、服务端要求关闭       -> false，归还时直接销毁
struct PooledHttpConn {
    PooledHttpConn() : stream(ioc) {}

    net::io_context ioc;        // 该连接专属的事件循环，仅用于连接阶段的异步 connect
    beast::tcp_stream stream;   // 真正收发 HTTP 数据的 stream
    bool reusable = true;       // 归还时是否放回池子
};

// HTTP 连接池。
//
// 内部池化的是 beast::tcp_stream，
// 而不是 VarifyService::Stub。
//
// 生命周期：
//   getConnection() -> 独占使用 -> returnConnection()
//
// 并发模型：
//   - idle_ 保存空闲连接；
//   - total_ 记录当前已创建（含正在使用）的连接数；
//   - 池空且 total_ < maxSize_ 时新建连接；
//   - 已达到 maxSize_ 时，调用线程在 cond_ 上等待，直到有人归还。
class HttpConPool {
public:
    // maxSize: 池中最多同时存在的连接数，也是并发上限
    // host/port: VerifyServer 地址
    // timeout: 连接和单次读写的超时时间
    HttpConPool(std::size_t maxSize,
                std::string host,
                std::string port,
                std::chrono::milliseconds timeout);

    ~HttpConPool();

    HttpConPool(const HttpConPool&) = delete;
    HttpConPool& operator=(const HttpConPool&) = delete;

    // 从池中取一条连接。
    //
    // 返回 nullptr 的三种情况：
    //   1. 池已 Close()；
    //   2. 新建连接失败（DNS/连接超时等）；
    //   3. 等待过程中池被关闭。
    //
    // 返回非空指针时，调用方拥有该连接，使用完必须 returnConnection()。
    std::unique_ptr<PooledHttpConn> getConnection();

    // 归还连接。
    //
    // - conn 为空：什么都不做；
    // - conn->reusable == false 或池已关闭：销毁连接，并减少 total_；
    // - 否则放回 idle_，唤醒一个正在等待 getConnection 的线程。
    void returnConnection(std::unique_ptr<PooledHttpConn> conn);

    // 关闭连接池。
    // 唤醒所有等待者，之后 getConnection 一律返回 nullptr。
    // 已在使用的连接由调用方归还时自动销毁。
    void Close();

private:
    // 新建一条连接并完成 TCP connect。
    // 成功返回连接；失败返回 nullptr（内部会打印错误日志）。
    std::unique_ptr<PooledHttpConn> createConnection();

    std::size_t maxSize_;               // 最大连接数
    std::size_t total_ = 0;             // 已创建连接数（空闲 + 使用中）
    std::string host_;                  // VerifyServer host
    std::string port_;                  // VerifyServer port
    std::chrono::milliseconds timeout_; // 超时时间

    std::queue<std::unique_ptr<PooledHttpConn>> idle_; // 空闲连接
    std::mutex mutex_;                                 // 保护 idle_ 和 total_
    std::condition_variable cond_;                     // 等待空闲连接/关闭通知
    std::atomic<bool> b_stop_{false};                  // 是否已关闭
};