#pragma once

#include "src/messages/message-queue.h"

#include "common/fibers/sync-primitives/shared-mutex.h"

namespace noctua {

class partition_t {
private:
  using message_queue_t = messages::message_queue_t;

public:
  using iterator = message_queue_t::iterator;
  using const_iterator = message_queue_t::const_iterator;

  using read_lock_t = common::fibers::shared_mutex_t::read_lock_guard_t;
  using write_lock_t = common::fibers::shared_mutex_t::write_lock_guard_t;

  partition_t() = default;
  partition_t(const partition_t&) = delete;
  partition_t(partition_t&&) = delete;

  partition_t& operator=(const partition_t&) = delete;
  partition_t& operator=(partition_t&&) = delete;

  ~partition_t() = default;

  [[nodiscard]] common::fibers::task_t<read_lock_t> read_lock() const noexcept {
    co_return mutex_.lock_shared();
  }

  [[nodiscard]] common::fibers::task_t<write_lock_t> write_lock() noexcept {
    co_return mutex_.lock();
  }

  const_iterator cbegin() const noexcept {
    return queue_.cbegin();
  }

  const_iterator cend() const noexcept {
    return queue_.cend();
  }

  iterator begin() noexcept {
    return queue_.begin();
  }

  iterator end() noexcept {
    return queue_.end();
  }

  const_iterator front() const noexcept {
    return queue_.front();
  }

  const_iterator back() const noexcept {
    return queue_.back();
  }

  iterator front() noexcept {
    return queue_.front();
  }

  iterator back() noexcept {
    return queue_.back();
  }

  iterator push(std::string_view msg) noexcept {
    return queue_.push(msg);
  }

  void pop() noexcept {
    return queue_.pop();
  }

  size_t size() const noexcept {
    return queue_.size();
  }

  bool empty() const noexcept {
    return queue_.empty();
  }

private:
  message_queue_t queue_;
  mutable common::fibers::shared_mutex_t mutex_;
};

} // namespace noctua
