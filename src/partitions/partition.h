#pragma once

#include "src/common/message.h"

#include "common/sync-primitives/shared-mutex.h"

#include <function2/function2.hpp>

namespace noctua::partitions {

class partition_t {
  using message_t = common::message_t;
  using message_ptr_t = common::message_t::message_ptr_t;

  [[nodiscard]] auto read_lock() const noexcept {
    return mutex_.scoped_lock_shared();
  }

  [[nodiscard]] auto write_lock() noexcept {
    return mutex_.scoped_lock();
  }

  void push(message_ptr_t message) noexcept {
    messages_.push(std::move(message));
  }

  void pop() noexcept {
    messages_.pop();
  }

  const message_t& front() const noexcept {
    return *messages_.front();
  }

  const message_t& back() const noexcept {
    return *messages_.back();
  }

private:
  std::queue<message_ptr_t> messages_;
  mutable ::common::fibers::shared_mutex_t mutex_;
};

} // namespace noctua::partitions
