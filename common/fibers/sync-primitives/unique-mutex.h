#pragma once

#include "common/fibers/task.h"
#include <coroutine>
#include <cstdint>
#include <deque>
#include <limits>
#include <mutex>

namespace common::fibers {

class unique_mutex_t {
public:
  class lock_guard_t {
  public:
    lock_guard_t() = delete;
    lock_guard_t(const lock_guard_t&) = delete;

    lock_guard_t(lock_guard_t&& other) noexcept
        : owned_(other.owned_), mutex_(other.mutex_) {
      other.owned_ = false;
    }

    explicit lock_guard_t(unique_mutex_t* mutex)
        : mutex_(mutex) {}

    lock_guard_t& operator=(const lock_guard_t&) = delete;

    lock_guard_t& operator=(lock_guard_t&& other) noexcept {
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

    ~lock_guard_t() {
      if (owned_ && mutex_) {
        mutex_->unlock();
      }
    }

  private:
    bool owned_{true};
    unique_mutex_t* mutex_;

    friend class unique_mutex_t;
  };

  common::fibers::task_t<lock_guard_t> lock() {
    while (!try_lock()) {
      co_await boost::asio::post(boost::asio::use_awaitable);
    }
    co_return lock_guard_t(this);
  }

private:
  bool try_lock() noexcept {
    return !locked_.exchange(true, std::memory_order_acquire);
  }

  void unlock() {
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

  friend class lock_guard_t;
};

} // namespace common::fibers
