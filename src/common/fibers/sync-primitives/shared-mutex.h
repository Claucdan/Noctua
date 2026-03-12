#pragma once

#include "common/fibers/task.h"

#include <deque>
#include <mutex>
#include <utility>

#include <function2/function2.hpp>

namespace noctua::fibers {

class shared_mutex_t {
private:
  template<typename CompletionToken>
  auto async_lock(CompletionToken&& token) {
    return boost::asio::async_initiate<CompletionToken, void(write_lock_guard_t)>(
            [this](auto handler) {
              using handler_type = decltype(handler);
              auto h = std::make_shared<handler_type>(std::move(handler));

              std::unique_lock<std::mutex> lock(state_mutex_);
              if (!writer_active_ && reader_count_ == 0) {
                writer_active_ = true;
                lock.unlock();
                boost::asio::post(boost::asio::get_associated_executor(*h),
                                  [this, h]() mutable { (*h)(write_lock_guard_t(*this)); });
              } else {
                ++writers_waiting_;
                unique_waiters_.emplace_back([this, h]() mutable { (*h)(write_lock_guard_t(*this)); });
              }
            },
            std::forward<CompletionToken>(token));
  }

  template<typename CompletionToken>
  auto async_lock_shared(CompletionToken&& token) {
    return boost::asio::async_initiate<CompletionToken, void(read_lock_guard_t)>(
            [this](auto handler) {
              using handler_type = decltype(handler);
              auto h = std::make_shared<handler_type>(std::move(handler));

              std::unique_lock<std::mutex> lock(state_mutex_);
              if (!writer_active_ && writers_waiting_ == 0) {
                ++reader_count_;
                lock.unlock();

                boost::asio::post(boost::asio::get_associated_executor(*h),
                                  [this, h]() mutable { (*h)(read_lock_guard_t(*this)); });
              } else {
                shared_waiters_.emplace_back([this, h]() mutable { (*h)(read_lock_guard_t(*this)); });
              }
            },
            std::forward<CompletionToken>(token));
  }

public:
  class write_lock_guard_t {
  public:
    write_lock_guard_t() = delete;
    write_lock_guard_t(const write_lock_guard_t&) = delete;

    write_lock_guard_t(write_lock_guard_t&& other) noexcept
        : mutex_(std::exchange(other.mutex_, nullptr)) {}

    explicit write_lock_guard_t(shared_mutex_t& mutex)
        : mutex_(&mutex) {}

    write_lock_guard_t& operator=(const write_lock_guard_t&) = delete;

    write_lock_guard_t& operator=(write_lock_guard_t&& other) noexcept {
      if (this != &other) {
        unlock();
        mutex_ = std::exchange(other.mutex_, nullptr);
      }
      return *this;
    }

    ~write_lock_guard_t() {
      unlock();
    }

    void unlock() noexcept {
      if (mutex_) {
        mutex_->unlock();
        mutex_ = nullptr;
      }
    }

  private:
    shared_mutex_t* mutex_{nullptr};
  };

  class read_lock_guard_t {
  public:
    read_lock_guard_t() = delete;
    read_lock_guard_t(const read_lock_guard_t&) = delete;

    read_lock_guard_t(read_lock_guard_t&& other) noexcept
        : mutex_(std::exchange(other.mutex_, nullptr)) {}

    explicit read_lock_guard_t(shared_mutex_t& mutex)
        : mutex_(&mutex) {}

    read_lock_guard_t& operator=(const read_lock_guard_t&) = delete;

    read_lock_guard_t& operator=(read_lock_guard_t&& other) noexcept {
      if (this != &other) {
        unlock();
        mutex_ = std::exchange(other.mutex_, nullptr);
      }
      return *this;
    }

    ~read_lock_guard_t() {
      unlock();
    }

    void unlock() {
      if (mutex_) {
        mutex_->unlock_shared();
        mutex_ = nullptr;
      }
    }

  private:
    shared_mutex_t* mutex_;
  };

  fibers::task_t<write_lock_guard_t> lock() {
    co_return co_await async_lock(boost::asio::use_awaitable);
  }

  fibers::task_t<read_lock_guard_t> lock_shared() {
    co_return co_await async_lock_shared(boost::asio::use_awaitable);
  }

private:
  void unlock() {
    std::unique_lock<std::mutex> lock(state_mutex_);
    writer_active_ = false;

    if (!unique_waiters_.empty()) {
      auto fn = std::move(unique_waiters_.front());
      unique_waiters_.pop_front();
      --writers_waiting_;
      writer_active_ = true;
      lock.unlock();
      fn();
    } else {
      while (!shared_waiters_.empty()) {
        auto fn = std::move(shared_waiters_.front());
        shared_waiters_.pop_front();
        ++reader_count_;
        lock.unlock();
        fn();
        lock.lock();
      }
    }
  }

  void unlock_shared() {
    std::unique_lock<std::mutex> lock(state_mutex_);

    if (--reader_count_ == 0 && !unique_waiters_.empty()) {
      auto fn = std::move(unique_waiters_.front());
      unique_waiters_.pop_front();
      --writers_waiting_;
      writer_active_ = true;
      lock.unlock();
      fn();
    }
  }

  std::mutex state_mutex_;

  bool writer_active_ = false;
  std::size_t reader_count_ = 0;
  std::size_t writers_waiting_ = 0;

  std::deque<fu2::function<void()>> shared_waiters_;
  std::deque<fu2::function<void()>> unique_waiters_;

  friend class read_lock_guard_t;
  friend class write_lock_guard_t;
};

} // namespace noctua::fibers
