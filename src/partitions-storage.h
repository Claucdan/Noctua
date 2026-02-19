#pragma once

#include "src/partition.h"

#include "common/kassert.h"

#include <concepts>
#include <cstdint>

namespace noctua {

class partititons_storage_t {
public:
  partititons_storage_t() = delete;
  partititons_storage_t(const partititons_storage_t& other) = delete;
  partititons_storage_t(partititons_storage_t&& other) noexcept = delete;

  explicit partititons_storage_t(size_t partitions_count)
      : partitions_(partitions_count) {}

  partititons_storage_t& operator=(const partititons_storage_t& other) = default;
  partititons_storage_t& operator=(partititons_storage_t&& other) noexcept = default;

  ~partititons_storage_t() = default;

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
