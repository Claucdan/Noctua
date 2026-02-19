#pragma once

#include <cstdint>
#include <string_view>

namespace noctua::wall {

struct partition_push_event_t {
  uint32_t idx;
  std::string_view data;
};

struct partition_pop_event_t {
  uint32_t idx;
  uint64_t offset;
};

}; // namespace noctua::wall
