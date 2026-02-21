#pragma once

#include "src/partition_t.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace noctua {

class topic_t {
public:
  topic_t() = delete;
  topic_t(const topic_t& other) = delete;
  topic_t(topic_t&& other) noexcept = delete;

  explicit topic_t(size_t partitions_count)
      : partitions_(partitions_count) {}

  topic_t& operator=(const topic_t& other) = default;
  topic_t& operator=(topic_t&& other) noexcept = default;

  ~topic_t() = default;

  [[nodiscard]] auto read_lock() const noexcept {
    return mutex_.scoped_lock_shared();
  }

  [[nodiscard]] auto write_lock() noexcept {
    return mutex_.scoped_lock();
  }

  [[nodiscard]] partition_t& get(uint32_t idx) noexcept {
    kassert_lt(idx, partitions_.size());
    return *partitions_[idx];
  }

  template<typename T, std::invocable<T> Hash = std::hash<T>>
  [[nodiscard]] partition_t& get(T data) noexcept {
    uint64_t hash = Hash(data);
    return *partitions_[hash % partitions_.size()];
  }

  [[nodiscard]] size_t size() const noexcept {
    return partitions_.size();
  }

private:
  using partition_ptr_t = std::unique_ptr<partition_t>;

  std::vector<partition_ptr_t> partitions_;
  mutable ::common::fibers::shared_mutex_t mutex_;
};

} // namespace noctua
