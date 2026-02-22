#pragma once

#include "common/fibers/task.h"

#include <coroutine>
#include <cstdint>
#include <deque>
#include <limits>
#include <mutex>

namespace common::fibers {

class shared_mutex_t {
public:
  class write_lock_guard_t {
  public:
    write_lock_guard_t() = delete;
    write_lock_guard_t(const write_lock_guard_t&) = delete;

    write_lock_guard_t(write_lock_guard_t&& other) noexcept
        : owned_(other.owned_), mutex_(other.mutex_) {
      other.owned_ = false;
    }

    explicit write_lock_guard_t(shared_mutex_t* mutex)
        : mutex_(mutex) {}

    write_lock_guard_t& operator=(const write_lock_guard_t&) = delete;

    write_lock_guard_t& operator=(write_lock_guard_t&& other) noexcept {
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

    ~write_lock_guard_t() {
      if (owned_ && mutex_) {
        mutex_->unlock();
      }
    }

  private:
    bool owned_{true};
    shared_mutex_t* mutex_;

    friend class shared_mutex_t;
  };

  class read_lock_guard_t {
  public:
    read_lock_guard_t() = delete;
    read_lock_guard_t(const read_lock_guard_t&) = delete;

    read_lock_guard_t(read_lock_guard_t&& other) noexcept
        : owned_(other.owned_), mutex_(other.mutex_) {
      other.owned_ = false;
    }

    explicit read_lock_guard_t(shared_mutex_t* mutex)
        : mutex_(mutex) {}

    read_lock_guard_t& operator=(const read_lock_guard_t&) = delete;

    read_lock_guard_t& operator=(read_lock_guard_t&& other) noexcept {
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

    ~read_lock_guard_t() {
      if (owned_ && mutex_) {
        mutex_->unlock_shared();
      }
    }

  private:
    bool owned_{true};
    shared_mutex_t* mutex_;

    friend class shared_mutex_t;
  };

  common::fibers::task_t<write_lock_guard_t> lock() {
    co_await boost::asio::post(boost::asio::use_awaitable);
    if (!try_lock()) {
      co_await boost::asio::post(boost::asio::use_awaitable);
    }
    co_return write_lock_guard_t(this);
  }

  common::fibers::task_t<read_lock_guard_t> lock_shared() {
    co_await boost::asio::post(boost::asio::use_awaitable);
    if (!try_lock_shared()) {
      co_await boost::asio::post(boost::asio::use_awaitable);
    }
    co_return read_lock_guard_t(this);
  }

private:
  bool try_lock() noexcept {
    uint32_t expected = 0;
    return state_.compare_exchange_strong(expected, WRITE_LOCKED, std::memory_order_acquire);
  }

  bool try_lock_shared() noexcept {
    uint32_t expected = state_.load(std::memory_order_relaxed);
    while (expected != WRITE_LOCKED) {
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
    uint32_t old_state = state_.fetch_sub(1, std::memory_order_release);
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

  static constexpr uint32_t WRITE_LOCKED = std::numeric_limits<uint32_t>::max();

  std::atomic<uint32_t> state_{0}; // max() = write locked, 0 = unlocked, >0 = N readers
  std::mutex waiters_mutex_;
  std::deque<std::coroutine_handle<>> write_waiters_;
  std::deque<std::coroutine_handle<>> shared_waiters_;

  friend class read_lock_guard_t;
  friend class write_lock_guard_t;
};

} // namespace common::fibers
