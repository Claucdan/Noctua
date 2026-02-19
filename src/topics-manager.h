#pragma once

#include "src/partitions/partition.h"
#include "src/partitions/partitions-storage.h"

#include "common/fibers/values.h"
#include "src/common/concepts.h"

#include <function2/function2.hpp>

namespace noctua {

class topics_manager_t {
public:
  topics_manager_t() = delete;
  topics_manager_t(const topics_manager_t& other) = delete;
  topics_manager_t(topics_manager_t&& other) = delete;

  explicit topics_manager_t(size_t partitions_count)
      : partititons_storage_(partitions_count) {}

  topics_manager_t& operator=(const topics_manager_t& other) = delete;
  topics_manager_t& operator=(topics_manager_t&& other) = delete;

  ~topics_manager_t() = default;

  ::common::fibers::task_t<void> add_message(std::string_view message, std::optional<uint32_t> idx) {
    partitions::partition_t* partition;
    auto message_ptr = common::message_t::create_message(message);
    if (idx.has_value()) {
      partition = &partititons_storage_.get(idx);
    } else {
      partition = &partititons_storage_.get(message);
    }

    co_return;
  }

private:
  wall_writer_;
  partitions::partititons_storage_t partititons_storage_;
};

} // namespace noctua
