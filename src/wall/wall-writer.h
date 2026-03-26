#pragma once

#include "common/kassert.h"

#include "common/fibers/task.h"
#include "common/fibers/sync-primitives/unique-mutex.h"

#include <fstream>
#include <span>
#include <string>
#include <string_view>

namespace noctua::wall {

class wall_writer_t {
public:
  wall_writer_t() = delete;
  wall_writer_t(const wall_writer_t&) = delete;
  wall_writer_t(wall_writer_t&&) = delete;

  explicit wall_writer_t(std::string_view file_path)
      : file_(std::string("/tmp/").append(file_path), std::ios::binary | std::ios::out) {
    kassert(file_.is_open(), "file should be open: [/tmp/{}]", file_path);
  }

  wall_writer_t& operator=(const wall_writer_t&) = delete;
  wall_writer_t& operator=(wall_writer_t&&) = delete;

  ~wall_writer_t() = default;

  void store(const void* raw_data, size_t data_size) {
    file_.write(static_cast<const char*>(raw_data), data_size);
    file_.flush();
  }

  void store(std::span<const std::byte> data) {
    file_.write(reinterpret_cast<const char*>(data.data()), data.size());
    file_.flush();
  }

private:
  std::ofstream file_;
  fibers::unique_mutex_t mutex_;
};

} // namespace noctua::wall
