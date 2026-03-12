#pragma once

#include "common/fibers/task.h"

#include <mutex>
#include <deque>
#include <utility>

#include <function2/function2.hpp>

namespace noctua::fibers {

class unique_mutex_t {
private:
  template<typename CompletionToken>
  auto async_lock(CompletionToken&& token) {
    return boost::asio::async_initiate<CompletionToken, void(lock_guard_t)>(
            [this](auto handler) {
              using handler_type = decltype(handler);
              auto h = std::make_shared<handler_type>(std::move(handler));
              auto ex = boost::asio::get_associated_executor(*h);

              std::unique_lock<std::mutex> lock(waiters_mutex_);

              if (!locked_) {
                locked_ = true;
                lock.unlock();

                boost::asio::post(ex, [this, h]() mutable { (*h)(lock_guard_t(*this)); });
              } else {
                waiters_.emplace_back([this, h]() mutable { (*h)(lock_guard_t(*this)); });
              }
            },
            std::forward<CompletionToken>(token));
  }

public:
  class lock_guard_t {
  public:
    lock_guard_t() = delete;
    lock_guard_t(const lock_guard_t&) = delete;

    lock_guard_t(lock_guard_t&& other) noexcept
        : mutex_(std::exchange(other.mutex_, nullptr)) {}

    explicit lock_guard_t(unique_mutex_t& mutex)
        : mutex_(&mutex) {}

    lock_guard_t& operator=(const lock_guard_t&) = delete;

    lock_guard_t& operator=(lock_guard_t&& other) noexcept {
      if (this != &other) {
        unlock();
        mutex_ = std::exchange(other.mutex_, nullptr);
      }
      return *this;
    }

    ~lock_guard_t() {
      unlock();
    }

    void unlock() noexcept {
      if (mutex_) {
        mutex_->unlock();
        mutex_ = nullptr;
      }
    }

  private:
    unique_mutex_t* mutex_;

    friend class unique_mutex_t;
  };

  fibers::task_t<lock_guard_t> lock() {
    co_return co_await async_lock(boost::asio::use_awaitable);
  }

private:
  void unlock() {
    std::unique_lock<std::mutex> lock(waiters_mutex_);

    if (waiters_.empty()) {
      locked_ = false;
      return;
    }

    auto fn = std::move(waiters_.front());
    waiters_.pop_front();
    lock.unlock();

    fn();
  }

  bool locked_{false};
  std::mutex waiters_mutex_;
  std::deque<fu2::function<void()>> waiters_;

  friend class lock_guard_t;
};

} // namespace noctua::fibers
