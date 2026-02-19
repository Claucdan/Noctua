#pragma once

#include <coroutine>
#include <deque>
#include <mutex>
#include <vector>

namespace common::fibers {

class shared_mutex_t {
public:
  shared_mutex_t() = default;
  shared_mutex_t(const shared_mutex_t&) = delete;
  shared_mutex_t(shared_mutex_t&&) noexcept = delete;

  shared_mutex_t& operator=(const shared_mutex_t&) = delete;
  shared_mutex_t& operator=(shared_mutex_t&&) noexcept = delete;

  ~shared_mutex_t() = default;

  class write_awaiter {
  public:
    explicit write_awaiter(shared_mutex_t& m)
        : m_(&m) {}

    write_awaiter(const write_awaiter&) = delete;
    write_awaiter& operator=(const write_awaiter&) = delete;

    bool await_ready() noexcept {
      std::lock_guard lock(m_->mutex_);
      if (!m_->writer_locked_ && m_->readers_ == 0) {
        m_->writer_locked_ = true;
        return true;
      }
      return false;
    }

    void await_suspend(std::coroutine_handle<> h) {
      std::lock_guard lock(m_->mutex_);
      m_->write_waiters_.push_back(h);
    }

    void await_resume() noexcept {}

  private:
    shared_mutex_t* m_;
  };

  class read_awaiter {
  public:
    explicit read_awaiter(shared_mutex_t& m)
        : m_(&m) {}

    read_awaiter(const read_awaiter&) = delete;
    read_awaiter& operator=(const read_awaiter&) = delete;

    bool await_ready() noexcept {
      std::lock_guard lock(m_->mutex_);
      if (!m_->writer_locked_ && m_->write_waiters_.empty()) {
        ++m_->readers_;
        return true;
      }
      return false;
    }

    void await_suspend(std::coroutine_handle<> h) {
      std::lock_guard lock(m_->mutex_);
      m_->read_waiters_.push_back(h);
    }

    void await_resume() noexcept {}

  private:
    shared_mutex_t* m_;
  };

  class write_lock {
  public:
    write_lock() = delete;

    explicit write_lock(shared_mutex_t& m)
        : m_(&m) {}

    write_lock(const write_lock&) = delete;
    write_lock& operator=(const write_lock&) = delete;

    write_lock(write_lock&& other) noexcept
        : m_(other.m_) {
      other.m_ = nullptr;
    }

    write_lock& operator=(write_lock&& other) noexcept {
      if (this != &other) {
        if (m_)
          m_->unlock();
        m_ = other.m_;
        other.m_ = nullptr;
      }
      return *this;
    }

    ~write_lock() {
      if (m_)
        m_->unlock();
    }

  private:
    shared_mutex_t* m_;
  };

  class read_lock {
  public:
    read_lock() = delete;

    explicit read_lock(shared_mutex_t& m)
        : m_(&m) {}

    read_lock(const read_lock&) = delete;
    read_lock& operator=(const read_lock&) = delete;

    read_lock(read_lock&& other) noexcept
        : m_(other.m_) {
      other.m_ = nullptr;
    }

    read_lock& operator=(read_lock&& other) noexcept {
      if (this != &other) {
        if (m_)
          m_->unlock_shared();
        m_ = other.m_;
        other.m_ = nullptr;
      }
      return *this;
    }

    ~read_lock() {
      if (m_)
        m_->unlock_shared();
    }

  private:
    shared_mutex_t* m_;
  };

  auto scoped_lock() noexcept {
    struct awaiter {
      shared_mutex_t& m_;
      write_awaiter inner_;

      explicit awaiter(shared_mutex_t& m)
          : m_(m), inner_(m) {}

      bool await_ready() noexcept {
        return inner_.await_ready();
      }

      void await_suspend(std::coroutine_handle<> h) {
        inner_.await_suspend(h);
      }

      write_lock await_resume() noexcept {
        inner_.await_resume();
        return write_lock{m_};
      }
    };

    return awaiter{*this};
  }

  auto scoped_lock_shared() noexcept {
    struct awaiter {
      shared_mutex_t& m_;
      read_awaiter inner_;

      explicit awaiter(shared_mutex_t& m)
          : m_(m), inner_(m) {}

      bool await_ready() noexcept {
        return inner_.await_ready();
      }

      void await_suspend(std::coroutine_handle<> h) {
        inner_.await_suspend(h);
      }

      read_lock await_resume() noexcept {
        inner_.await_resume();
        return read_lock{m_};
      }
    };

    return awaiter{*this};
  }

private:
  void unlock() {
    std::vector<std::coroutine_handle<>> to_resume;

    {
      std::lock_guard lock(mutex_);

      writer_locked_ = false;

      if (!write_waiters_.empty()) {
        auto h = write_waiters_.front();
        write_waiters_.pop_front();
        writer_locked_ = true;
        to_resume.push_back(h);
      } else {
        readers_ = read_waiters_.size();
        for (auto h : read_waiters_)
          to_resume.push_back(h);
        read_waiters_.clear();
      }
    }

    for (auto h : to_resume)
      h.resume();
  }

  void unlock_shared() {
    std::coroutine_handle<> to_resume;

    {
      std::lock_guard lock(mutex_);

      --readers_;

      if (readers_ == 0 && !write_waiters_.empty()) {
        to_resume = write_waiters_.front();
        write_waiters_.pop_front();
        writer_locked_ = true;
      }
    }

    if (to_resume)
      to_resume.resume();
  }

private:
  std::mutex mutex_;
  bool writer_locked_ = false;
  size_t readers_ = 0;

  std::deque<std::coroutine_handle<>> read_waiters_;
  std::deque<std::coroutine_handle<>> write_waiters_;
};

} // namespace common::fibers
