#pragma once

#include "common/concepts.h"
#include "common/fibers/task.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace noctua::wall {

inline constexpr uint64_t WALL_HEADER_MAGIC = 0x9A2D5895;
inline constexpr uint64_t WALL_DATA_MAGIC = 0x4FE55045;
inline constexpr uint64_t WALL_FOOTER_MAGIC = 0x23BAA1CD;

struct wall_header_t {
  uint64_t magic = WALL_HEADER_MAGIC;
  uint64_t offset_id;
  std::string_view topic_name;
  uint64_t data_count;
};

struct wall_data_t {
  uint64_t magic = WALL_DATA_MAGIC;
  uint16_t identifier;
  bool is_push_operation;
  std::string_view message;
};

struct wall_footer_t {
  uint64_t magic = WALL_FOOTER_MAGIC;
  std::uint64_t hash_sum;
};

} // namespace noctua::wall
