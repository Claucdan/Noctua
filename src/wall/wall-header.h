#pragma once

#include "common/concepts.h"
#include "common/fibers/task.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace noctua::wall {

struct wall_header_t {
  static constexpr uint64_t magic = 0;

  template<common::wall_writer_t Writer>
  fibers::task_t<void> store(Writer& writer) {
    co_await writer.store(&magic, sizeof(magic));
    co_await writer.store(&offset_id, sizeof(offset_id));
    uint64_t topic_name_size = topic_name.size();
    co_await writer.store(&topic_name_size, sizeof(topic_name_size));
    co_await writer.store(std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(topic_name.data()), topic_name.size()));
    co_await writer.store(&data_count, sizeof(data_count));
  }

  uint64_t offset_id;
  std::string_view topic_name;
  uint64_t data_count;
};

struct wall_data_t {
  static constexpr uint64_t magic = 1;

  template<common::wall_writer_t Writer>
  fibers::task_t<void> store(Writer& writer) {
    co_await writer.store(&magic, sizeof(magic));
    co_await writer.store(&identifier, sizeof(identifier));
    co_await writer.store(&is_push_operation, sizeof(is_push_operation));
    uint64_t message_size = message.size();
    co_await writer.store(&message_size, sizeof(message_size));
    co_await writer.store(std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(message.data()), message.size()));
  }

  uint16_t identifier;
  bool is_push_operation;
  std::string_view message;
};

struct wall_footer_t {
  static constexpr uint64_t magic = 2;

  template<common::wall_writer_t Writer>
  fibers::task_t<void> store(Writer& writer) {
    co_await writer.store(&magic, sizeof(magic));
    co_await writer.store(&hash_sum, sizeof(hash_sum));
  }

  std::uint64_t hash_sum;
};

} // namespace noctua::wall
