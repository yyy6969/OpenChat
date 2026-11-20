#include "httpconpool.h"

#include <exception>
#include <iostream>
#include <utility>

#include <boost/asio/connect.hpp>
#include <boost/asio/steady_timer.hpp>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <sys/time.h>
#endif

namespace {

    // 给同步 socket 设置收发超时。
    //
    // 注意：beast::tcp_stream::expires_after 只对异步操作生效，
    // 同步的 http::read/http::write 最终走 socket 的同步读写，
    // 所以这里直接设置 SO_RCVTIMEO / SO_SNDTIMEO 才能真正限制阻塞时间。
    void SetSocketTimeout(tcp::socket& socket, std::chrono::milliseconds timeout) {
#ifdef _WIN32
        DWORD ms = static_cast<DWORD>(timeout.count());
        ::setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO,
                     reinterpret_cast<const char*>(&ms), sizeof(ms));
        ::setsockopt(socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO,
                     reinterpret_cast<const char*>(&ms), sizeof(ms));
#else
        timeval tv{};
        tv.tv_sec = static_cast<long>(timeout.count() / 1000);
        tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);
        ::setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        ::setsockopt(socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
    }

    // 检查一条空闲的 keep-alive 连接是否仍然可用。
    //
    // 为什么要检查：
    // VerifyServer 重启、主动关闭空闲连接、或网络断开时，池里保存的旧连接
    // 已经失效。如果直接拿它发请求，第一次会失败，需要靠重试恢复。
    // 这里在取出空闲连接时先用非阻塞 peek 看一眼：
    //   - 没有数据且没有错误 -> 连接正常；
    //   - 读到 0 字节 / EOF   -> 对端已关闭，丢弃；
    //   - 有可读数据          -> 可能是残留响应，状态不可信，丢弃。
    bool IsConnectionAlive(tcp::socket& socket) {
        beast::error_code ec;

        // 临时切成非阻塞，避免 peek 时卡住。
        socket.non_blocking(true, ec);
        if (ec) {
            // 无法切换就先认为连接可用，让后续读写去暴露问题。
            return true;
        }

        char byte = 0;
        std::size_t n = socket.receive(
            net::buffer(&byte, 1), net::socket_base::message_peek, ec);

        // 恢复阻塞模式，后续 http::read/http::write 依赖它。
        beast::error_code restore_ec;
        socket.non_blocking(false, restore_ec);
        if (restore_ec) {
            return false;
        }

        if (n > 0) {
            // 对端发了意料之外的数据，这条 keep-alive 连接状态不干净。
            return false;
        }
        if (ec == net::error::would_block || ec == net::error::try_again) {
            // 没有数据可读，说明连接空闲且健康。
            return true;
        }
        if (ec == net::error::eof) {
            return false;
        }
        if (ec) {
            return false;
        }
        // n == 0 且无错误：对端已关闭。
        return false;
    }

}  // namespace

HttpConPool::HttpConPool(std::size_t maxSize,
                         std::string host,
                         std::string port,
                         std::chrono::milliseconds timeout)
    : maxSize_(maxSize == 0 ? 1 : maxSize)
    , host_(std::move(host))
    , port_(std::move(port))
    , timeout_(timeout) {
}

HttpConPool::~HttpConPool() {
    // 先唤醒所有阻塞在 getConnection 的线程。
    Close();

    // 把空闲连接取出来，在锁外统一销毁，避免析构时长时间持锁。
    std::queue<std::unique_ptr<PooledHttpConn>> idle;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        idle.swap(idle_);
    }
    // idle 离开作用域时自动析构，socket 随之关闭。
}

std::unique_ptr<PooledHttpConn> HttpConPool::getConnection() {
    std::unique_lock<std::mutex> lock(mutex_);

    for (;;) {
        // 等待条件：池已关闭 / 有空闲连接 / 还能新建连接。
        cond_.wait(lock, [this] {
            return b_stop_.load() || !idle_.empty() || total_ < maxSize_;
        });

        if (b_stop_.load()) {
            return nullptr;
        }

        // 优先复用空闲连接，但先确认它没有被服务端关闭。
        if (!idle_.empty()) {
            auto conn = std::move(idle_.front());
            idle_.pop();

            if (IsConnectionAlive(conn->stream.socket())) {
                return conn;
            }

            // 旧连接已失效（例如 VerifyServer 重启），销毁并释放名额，
            // 继续尝试下一条空闲连接，或者走新建流程。
            --total_;
            continue;
        }

        break;
    }

    // 没有空闲连接，但还没到上限，先占一个名额再在锁外建立连接，
    // 避免 connect 阻塞时一直持有 mutex。
    ++total_;
    lock.unlock();

    auto conn = createConnection();
    if (!conn) {
        // 建立失败，释放刚才占用的名额，并唤醒一个等待者。
        lock.lock();
        --total_;
        lock.unlock();
        cond_.notify_one();
        return nullptr;
    }
    return conn;
}

void HttpConPool::returnConnection(std::unique_ptr<PooledHttpConn> conn) {
    if (!conn) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (b_stop_.load() || !conn->reusable) {
            // 坏连接或池已关闭：直接销毁，释放名额。
            --total_;
        } else {
            // 好连接放回空闲队列，等待下一次复用。
            idle_.push(std::move(conn));
        }
    }

    // 无论归还成功还是销毁，都可能有线程在等名额/等待连接。
    cond_.notify_one();
}

void HttpConPool::Close() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        b_stop_ = true;
    }
    // 唤醒所有等待者，让它们从 getConnection 返回 nullptr。
    cond_.notify_all();
}

std::unique_ptr<PooledHttpConn> HttpConPool::createConnection() {
    auto conn = std::make_unique<PooledHttpConn>();

    try {
        tcp::resolver resolver(conn->ioc);
        auto endpoints = resolver.resolve(host_, port_);

        beast::error_code connect_ec;
        bool timed_out = false;

        // 用异步 connect + 定时器实现连接超时。
        // 同步 socket.connect() 无法被 SO_RCVTIMEO/SO_SNDTIMEO 限制。
        net::steady_timer timer(conn->ioc);
        timer.expires_after(timeout_);
        timer.async_wait([&conn, &timed_out](const beast::error_code& ec) {
            if (!ec) {
                // 定时器先触发：主动关闭 socket，让 connect 以错误结束。
                timed_out = true;
                beast::error_code ignored;
                conn->stream.socket().close(ignored);
            }
        });

        net::async_connect(
            conn->stream.socket(), endpoints,
            [&connect_ec, &timer](const beast::error_code& ec, const tcp::endpoint&) {
                connect_ec = ec;
                // connect 结束（成功或失败）后取消定时器。
                timer.cancel();
            });

        // 驱动上面两个异步操作直到完成。
        conn->ioc.run();

        if (timed_out || connect_ec) {
            std::cerr << "HttpConPool connect " << host_ << ":" << port_
                      << " failed: "
                      << (timed_out ? "timeout" : connect_ec.message())
                      << std::endl;
            return nullptr;
        }

        // 连接成功后设置同步读写超时。
        SetSocketTimeout(conn->stream.socket(), timeout_);
        return conn;
    } catch (const std::exception& e) {
        std::cerr << "HttpConPool createConnection exception: " << e.what()
        << std::endl;
        return nullptr;
    }
}