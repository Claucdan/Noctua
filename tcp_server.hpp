#pragma once

#include <atomic>
#include <coroutine>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <fmt/core.h>

using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::post;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;

// AsyncMutex that works with co_await
class AsyncMutex {
public:
  class LockGuard {
  public:
    explicit LockGuard(AsyncMutex* mutex)
        : mutex_(mutex) {}

    ~LockGuard() {
      if (owned_ && mutex_) {
        mutex_->unlock();
      }
    }

    LockGuard(LockGuard&& other) noexcept
        : mutex_(other.mutex_), owned_(other.owned_) {
      other.owned_ = false;
    }

    LockGuard& operator=(LockGuard&& other) noexcept {
      if (this != &other) {
        if (owned_ && mutex_) {
          mutex_->unlock();
        }
        mutex_ = other.mutex_;
        owned_ = other.owned_;
        other.owned_ = false;
      }
      return *this;
    }

    void release() noexcept {
      owned_ = false;
    }

    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;

  private:
    AsyncMutex* mutex_;
    bool owned_ = true;

    friend class AsyncMutex;
  };

  awaitable<LockGuard> lock() {
    co_await post(use_awaitable);
    if (!try_lock()) {
      co_await post(use_awaitable);
    }
    co_return LockGuard(this);
  }

private:
  friend class LockGuard;

  bool try_lock() noexcept {
    return !locked_.exchange(true, std::memory_order_acquire);
  }

  void unlock() noexcept {
    std::unique_lock lock(waiters_mutex_);
    if (waiters_.empty()) {
      locked_.store(false, std::memory_order_release);
      return;
    }
    auto handle = waiters_.front();
    waiters_.pop_front();
    lock.unlock();
    handle.resume();
  }

  std::atomic<bool> locked_{false};
  std::mutex waiters_mutex_;
  std::deque<std::coroutine_handle<>> waiters_;
};

// AsyncSharedMutex - shared/read-write mutex with co_await support
class AsyncSharedMutex {
public:
  class WriteLockGuard {
  public:
    explicit WriteLockGuard(AsyncSharedMutex* mutex)
        : mutex_(mutex) {}

    ~WriteLockGuard() {
      if (owned_ && mutex_) {
        mutex_->unlock();
      }
    }

    WriteLockGuard(WriteLockGuard&& other) noexcept
        : mutex_(other.mutex_), owned_(other.owned_) {
      other.owned_ = false;
    }

    WriteLockGuard& operator=(WriteLockGuard&& other) noexcept {
      if (this != &other) {
        if (owned_ && mutex_) {
          mutex_->unlock();
        }
        mutex_ = other.mutex_;
        owned_ = other.owned_;
        other.owned_ = false;
      }
      return *this;
    }

    void release() noexcept {
      owned_ = false;
    }

    WriteLockGuard(const WriteLockGuard&) = delete;
    WriteLockGuard& operator=(const WriteLockGuard&) = delete;

  private:
    AsyncSharedMutex* mutex_;
    bool owned_ = true;

    friend class AsyncSharedMutex;
  };

  class SharedLockGuard {
  public:
    explicit SharedLockGuard(AsyncSharedMutex* mutex)
        : mutex_(mutex) {}

    ~SharedLockGuard() {
      if (owned_ && mutex_) {
        mutex_->unlock_shared();
      }
    }

    SharedLockGuard(SharedLockGuard&& other) noexcept
        : mutex_(other.mutex_), owned_(other.owned_) {
      other.owned_ = false;
    }

    SharedLockGuard& operator=(SharedLockGuard&& other) noexcept {
      if (this != &other) {
        if (owned_ && mutex_) {
          mutex_->unlock_shared();
        }
        mutex_ = other.mutex_;
        owned_ = other.owned_;
        other.owned_ = false;
      }
      return *this;
    }

    void release() noexcept {
      owned_ = false;
    }

    SharedLockGuard(const SharedLockGuard&) = delete;
    SharedLockGuard& operator=(const SharedLockGuard&) = delete;

  private:
    AsyncSharedMutex* mutex_;
    bool owned_ = true;

    friend class AsyncSharedMutex;
  };

  awaitable<WriteLockGuard> lock() {
    co_await post(use_awaitable);
    if (!try_lock()) {
      co_await post(use_awaitable);
    }
    co_return WriteLockGuard(this);
  }

  awaitable<SharedLockGuard> lock_shared() {
    co_await post(use_awaitable);
    if (!try_lock_shared()) {
      co_await post(use_awaitable);
    }
    co_return SharedLockGuard(this);
  }

private:
  friend class WriteLockGuard;
  friend class SharedLockGuard;

  bool try_lock() noexcept {
    int expected = 0;
    return state_.compare_exchange_strong(expected, WRITE_LOCKED, std::memory_order_acquire);
  }

  bool try_lock_shared() noexcept {
    int expected = state_.load(std::memory_order_relaxed);
    while (expected >= 0) {
      if (state_.compare_exchange_weak(expected, expected + 1, std::memory_order_acquire)) {
        return true;
      }
    }
    return false;
  }

  void unlock() noexcept {
    std::unique_lock lock(waiters_mutex_);
    if (write_waiters_.empty()) {
      state_.store(0, std::memory_order_release);
      resume_shared_waiters();
      return;
    }
    auto handle = write_waiters_.front();
    write_waiters_.pop_front();
    lock.unlock();
    handle.resume();
  }

  void unlock_shared() noexcept {
    std::unique_lock lock(waiters_mutex_);
    int old_state = state_.fetch_sub(1, std::memory_order_release);
    if (old_state == 1 && !write_waiters_.empty()) {
      // Last shared lock released, wake a writer
      auto handle = write_waiters_.front();
      write_waiters_.pop_front();
      lock.unlock();
      handle.resume();
    }
  }

  void resume_shared_waiters() noexcept {
    while (!shared_waiters_.empty()) {
      auto handle = shared_waiters_.front();
      shared_waiters_.pop_front();
      handle.resume();
    }
  }

  static constexpr int WRITE_LOCKED = -1;

  std::atomic<int> state_{0}; // -1 = write locked, 0 = unlocked, >0 = N readers
  std::mutex waiters_mutex_;
  std::deque<std::coroutine_handle<>> write_waiters_;
  std::deque<std::coroutine_handle<>> shared_waiters_;
};

class TcpSession {
public:
  explicit TcpSession(tcp::socket socket, std::function<void()> on_disconnect)
      : socket_(std::move(socket))
      , remote_endpoint_(socket_.remote_endpoint())
      , on_disconnect_(std::move(on_disconnect)) {
    fmt::print("[{}] Client connected: {}\n", get_thread_id(), remote_endpoint_.address().to_string());
  }

  ~TcpSession() {
    if (on_disconnect_) {
      on_disconnect_();
    }
    fmt::print("[{}] Client disconnected: {}\n", get_thread_id(), remote_endpoint_.address().to_string());
  }

  awaitable<void> run() {
    try {
      for (;;) {
        std::size_t length = co_await socket_.async_read_some(boost::asio::buffer(data_, max_length), use_awaitable);

        std::string_view message(data_, length);
        fmt::print("[{}] Received {} bytes from {}: {}\n",
                   get_thread_id(),
                   length,
                   remote_endpoint_.address().to_string(),
                   message);

        co_await boost::asio::async_write(socket_, boost::asio::buffer(data_, length), use_awaitable);

        fmt::print("[{}] Sent {} bytes to {}\n", get_thread_id(), length, remote_endpoint_.address().to_string());
      }
    } catch (const boost::system::system_error& e) {
    }
  }

private:
  [[nodiscard]] std::string get_thread_id() const {
    return fmt::format("Thread-{}", std::hash<std::thread::id>{}(std::this_thread::get_id()));
  }

  tcp::socket socket_;
  tcp::endpoint remote_endpoint_;
  std::function<void()> on_disconnect_;

  enum {
    max_length = 1024
  };

  char data_[max_length];
};

class TcpServer {
public:
  explicit TcpServer(boost::asio::io_context& io_context, unsigned short port)
      : io_context_(io_context), acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
    fmt::print("Server listening on port {}\n", port);
    do_accept();
  }

  ~TcpServer() {
    fmt::print("Server stopped\n");
  }

  TcpServer(const TcpServer&) = delete;
  TcpServer& operator=(const TcpServer&) = delete;
  TcpServer(TcpServer&&) = delete;
  TcpServer& operator=(TcpServer&&) = delete;

private:
  void do_accept() {
    co_spawn(
            io_context_,
            [this]() -> awaitable<void> {
              for (;;) {
                tcp::socket socket = co_await acceptor_.async_accept(use_awaitable);

                {
                  auto lock = co_await connections_mutex_.lock();
                  ++active_connections_;
                  fmt::print("Active connections: {}\n", active_connections_.load());
                  lock.release(); // unlock
                }

                auto on_disconnect = [this]() -> void {
                  --active_connections_;
                  fmt::print("Active connections: {}\n", active_connections_.load());
                };

                auto session = std::make_shared<TcpSession>(std::move(socket), on_disconnect);
                co_spawn(io_context_, [session]() -> awaitable<void> { co_await session->run(); }, detached);
              }
            },
            detached);
  }

  boost::asio::io_context& io_context_;
  tcp::acceptor acceptor_;
  AsyncSharedMutex connections_mutex_;
  std::atomic<std::size_t> active_connections_{0};
};

class MultiThreadedTcpServer {
public:
  explicit MultiThreadedTcpServer(unsigned short port, std::size_t thread_count = std::thread::hardware_concurrency())
      : work_guard_(boost::asio::make_work_guard(io_context_))
      , server_(io_context_, port)
      , thread_count_(thread_count) {}

  void run() {
    fmt::print("Starting {} worker threads\n", thread_count_);
    threads_.reserve(thread_count_);
    for (std::size_t i = 0; i < thread_count_; ++i) {
      threads_.emplace_back([this] { io_context_.run(); });
    }
  }

  void stop() {
    fmt::print("Stopping server...\n");
    work_guard_.reset();
    for (auto& thread : threads_) {
      if (thread.joinable()) {
        thread.join();
      }
    }
    io_context_.stop();
  }

  ~MultiThreadedTcpServer() {
    stop();
  }

  MultiThreadedTcpServer(const MultiThreadedTcpServer&) = delete;
  MultiThreadedTcpServer& operator=(const MultiThreadedTcpServer&) = delete;
  MultiThreadedTcpServer(MultiThreadedTcpServer&&) = delete;
  MultiThreadedTcpServer& operator=(MultiThreadedTcpServer&&) = delete;

private:
  boost::asio::io_context io_context_;
  boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
  TcpServer server_;
  std::size_t thread_count_;
  std::vector<std::thread> threads_;
};
