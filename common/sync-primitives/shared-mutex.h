#pragma once

#include <coroutine>
#include <deque>
#include <mutex>

#include <boost/cobalt.hpp>

namespace common::fibers {

class shared_mutex_t {
public:
  struct write_awaiter_t {
    write_awaiter_t() = delete;
    write_awaiter_t(const write_awaiter_t& other) = default;
    write_awaiter_t(write_awaiter_t&& other) noexcept = default;

    write_awaiter_t(shared_mutex_t& mutex)
        : mutex_(&mutex) {}

    write_awaiter_t& operator=(const write_awaiter_t& other) = default;
    write_awaiter_t& operator=(write_awaiter_t&& other) noexcept = default;

    ~write_awaiter_t() = default;

    bool await_ready() noexcept {
      return false;
    }

    void await_suspend(std::coroutine_handle<> handle) {
      handle_ = handle;
      std::lock_guard lock(mutex_->mutex_);

      if (!mutex_->writer_locked_ && mutex_->readers_ == 0) {
        mutex_->writer_locked_ = true;
        handle_.resume();
      } else {
        mutex_->write_waiters_.push_back(this);
      }
    }

    void await_resume() noexcept {}

    shared_mutex_t* mutex_;
    std::coroutine_handle<> handle_;
  };

  struct read_awaiter_t {
    read_awaiter_t() = default;
    read_awaiter_t(const read_awaiter_t& other) = default;
    read_awaiter_t(read_awaiter_t&& other) noexcept = default;

    read_awaiter_t(shared_mutex_t& mutex)
        : mutex_(&mutex) {}

    read_awaiter_t& operator=(const read_awaiter_t& other) = default;
    read_awaiter_t& operator=(read_awaiter_t&& other) noexcept = default;

    ~read_awaiter_t() = default;

    bool await_ready() noexcept {
      return false;
    }

    void await_suspend(std::coroutine_handle<> handle) {
      handle_ = handle;
      std::lock_guard lock(mutex_->mutex_);

      if (!mutex_->writer_locked_ && mutex_->write_waiters_.empty()) {
        mutex_->readers_++;
        handle_.resume();
      } else {
        mutex_->read_waiters_.push_back(this);
      }
    }

    void await_resume() noexcept {}

    shared_mutex_t* mutex_;
    std::coroutine_handle<> handle_;
  };

  shared_mutex_t() = default;
  shared_mutex_t(const shared_mutex_t& other) = delete;
  shared_mutex_t(shared_mutex_t&& other) = delete;

  shared_mutex_t& operator=(const shared_mutex_t& other) = delete;
  shared_mutex_t& operator=(shared_mutex_t&& other) = delete;

  ~shared_mutex_t() = default;

  auto lock() noexcept {
    return write_awaiter_t{*this};
  }

  auto lock_shared() noexcept {
    return read_awaiter_t{*this};
  }

  auto scoped_lock() noexcept;
  auto scoped_lock_shared() noexcept;

  void unlock() noexcept {
    std::lock_guard lock(mutex_);
    writer_locked_ = false;

    if (!write_waiters_.empty()) {
      auto* next = write_waiters_.front();
      write_waiters_.pop_front();
      writer_locked_ = true;
      next->handle_.resume();
      return;
    }

    for (auto* waiter : read_waiters_) {
      waiter->handle_.resume();
    }

    read_waiters_.clear();
    readers_ = static_cast<int>(read_waiters_.size());
  }

  void unlock_shared() noexcept {
    std::lock_guard lock(mutex_);
    readers_--;

    if (readers_ == 0 && !write_waiters_.empty()) {
      auto* next = write_waiters_.front();
      write_waiters_.pop_front();
      writer_locked_ = true;
      next->handle_.resume();
    }
  }

private:
  mutable std::mutex mutex_;
  bool writer_locked_ = false;
  int readers_ = 0;

  std::deque<read_awaiter_t*> read_waiters_;
  std::deque<write_awaiter_t*> write_waiters_;
};

class write_lock_t {
public:
  class write_lock_awaiter_t {
  public:
    write_lock_awaiter_t() = delete;
    write_lock_awaiter_t(const write_lock_awaiter_t& other) = default;
    write_lock_awaiter_t(write_lock_awaiter_t&& other) noexcept = default;

    write_lock_awaiter_t(shared_mutex_t& mutex)
        : inner_(mutex) {}

    write_lock_awaiter_t& operator=(const write_lock_awaiter_t& other) = default;
    write_lock_awaiter_t& operator=(write_lock_awaiter_t&& other) noexcept = default;

    ~write_lock_awaiter_t() = default;

    bool await_ready() noexcept {
      return inner_.await_ready();
    }

    void await_suspend(std::coroutine_handle<> handle) {
      inner_.await_suspend(handle);
    }

    write_lock_t await_resume() noexcept {
      inner_.await_resume();
      return write_lock_t(*inner_.mutex_);
    }

  private:
    shared_mutex_t::write_awaiter_t inner_;
  };

  write_lock_t() = default;
  write_lock_t(const write_lock_t&) = delete;
  write_lock_t(write_lock_t&&) noexcept = default;

  explicit write_lock_t(shared_mutex_t& mutex) noexcept
      : mutex_(&mutex) {}

  write_lock_t& operator=(const write_lock_t&) = delete;

  write_lock_t& operator=(write_lock_t&& other) noexcept {
    if (this != &other) {
      if (mutex_) {
        mutex_->unlock();
      }
      mutex_ = other.mutex_;
      other.mutex_ = nullptr;
    }
    return *this;
  }

  ~write_lock_t() noexcept {
    if (mutex_) {
      mutex_->unlock();
    }
  }

private:
  shared_mutex_t* mutex_;
};

class read_lock_t {
public:
  class read_lock_awaiter_t {
  public:
    read_lock_awaiter_t() = default;
    read_lock_awaiter_t(const read_lock_awaiter_t& other) = default;
    read_lock_awaiter_t(read_lock_awaiter_t&& other) noexcept = default;

    read_lock_awaiter_t(shared_mutex_t& mutex)
        : inner_(mutex) {}

    read_lock_awaiter_t& operator=(const read_lock_awaiter_t& other) = default;
    read_lock_awaiter_t& operator=(read_lock_awaiter_t&& other) noexcept = default;

    ~read_lock_awaiter_t() = default;

    bool await_ready() noexcept {
      return inner_.await_ready();
    }

    void await_suspend(std::coroutine_handle<> handle) {
      inner_.await_suspend(handle);
    }

    read_lock_t await_resume() noexcept {
      inner_.await_resume();
      return read_lock_t(*inner_.mutex_);
    }

  private:
    shared_mutex_t::read_awaiter_t inner_;
  };

  read_lock_t() = delete;
  read_lock_t(const read_lock_t&) = delete;
  read_lock_t(read_lock_t&& other) noexcept = default;

  explicit read_lock_t(shared_mutex_t& mtx) noexcept
      : mtx_(&mtx) {}

  read_lock_t& operator=(const read_lock_t&) = delete;

  read_lock_t& operator=(read_lock_t&& other) noexcept {
    if (this != &other) {
      if (mtx_) {
        mtx_->unlock_shared();
      }
      mtx_ = other.mtx_;
      other.mtx_ = nullptr;
    }
    return *this;
  }

  ~read_lock_t() noexcept {
    if (mtx_) {
      mtx_->unlock_shared();
    }
  }

private:
  shared_mutex_t* mtx_;
};

inline auto shared_mutex_t::scoped_lock() noexcept {
  return write_lock_t::write_lock_awaiter_t{*this};
}

inline auto shared_mutex_t::scoped_lock_shared() noexcept {
  return read_lock_t::read_lock_awaiter_t{*this};
}

} // namespace common::fibers
