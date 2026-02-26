#pragma once

#include "partition.h"
#include "common/constants.h"
#include "common/request.h"

#include "common/kassert.h"
#include "common/fibers/task.h"
#include "common/fibers/sync-primitives/shared-mutex.h"
#include "wall/wall-header.h"
#include "wall/wall-writer.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace noctua {

template<class HashFunc>
class topic_t {
public:
  using read_lock_t = fibers::shared_mutex_t::read_lock_guard_t;
  using write_lock_t = fibers::shared_mutex_t::write_lock_guard_t;

  topic_t() = delete;
  topic_t(const topic_t&) = delete;
  topic_t(topic_t&&) = delete;

  explicit topic_t(size_t partitions_count,
                   std::string_view wall_path,
                   std::string_view topic_name,
                   HashFunc hash_func = HashFunc{})
      : wall_write_(wall_path), hash_func_(std::move(hash_func)), topic_name_(topic_name) {
    kassert_lt(partitions_count, common::INVALID_TOPIC_ID);
    storage_.reserve(partitions_count);
    for (size_t i = 0; i < partitions_count; ++i) {
      storage_.emplace_back(std::make_unique<partition_t>());
    }
  }

  topic_t& operator=(const topic_t&) = delete;
  topic_t& operator=(topic_t&&) = delete;

  ~topic_t() = default;

  [[nodiscard]] fibers::task_t<read_lock_t> read_lock() const noexcept {
    co_return co_await mutex_.lock_shared();
  }

  [[nodiscard]] fibers::task_t<write_lock_t> write_lock() noexcept {
    co_return co_await mutex_.lock();
  }

  fibers::task_t<void> push(const common::request_t& request) {
    uint16_t partition_idx = request.partition_id();

    if (partition_idx == common::INVALID_TOPIC_ID) {
      partition_idx = hash_func_(request) % storage_.size();
    }

    wall::wall_data_t wall_data;
    wall_data.identifier = partition_idx;
    wall_data.is_push_operation = true;
    wall_data.message = request.message_view();

    auto lock = co_await storage_[partition_idx]->write_lock();
    co_await wall_data.store(wall_write_);
    storage_[partition_idx]->push(request.message_view());
    co_return;
  }

  fibers::task_t<std::optional<std::string>> read(uint16_t partition_id) {
    kassert_ne(partition_id, common::INVALID_TOPIC_ID);

    auto lock = co_await storage_[partition_id]->read_lock();
    if (storage_[partition_id]->empty()) {
      co_return std::nullopt;
    }
    auto message_it = storage_[partition_id]->front();
    co_return std::string{message_it->get_data()};
  }

  fibers::task_t<bool> remove(const common::request_t& request) {
    uint16_t partition_idx = request.partition_id();

    if (partition_idx == common::INVALID_TOPIC_ID) {
      partition_idx = hash_func_(request) % storage_.size();
    }

    wall::wall_data_t wall_data;
    wall_data.identifier = partition_idx;
    wall_data.is_push_operation = false;
    wall_data.message = request.message_view();

    {
      auto lock = co_await storage_[partition_idx]->read_lock();
      auto message_it = storage_[partition_idx]->front();
      if (message_it->get_data() != request.message_view()) {
        co_return false;
      }
    }

    auto lock = co_await storage_[partition_idx]->read_lock();
    auto message_it = storage_[partition_idx]->front();
    if (message_it->get_data() != request.message_view()) {
      co_return false;
    }
    co_await wall_data.store(wall_write_);
    storage_[partition_idx]->pop();

    co_return;
  }

private:
  wall::wall_writer_t wall_write_;
  HashFunc hash_func_;
  std::string_view topic_name_;
  std::uint64_t offset_id_{0};
  std::vector<std::unique_ptr<partition_t>> storage_;
  mutable fibers::shared_mutex_t mutex_;
};

} // namespace noctua
