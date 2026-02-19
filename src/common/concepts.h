#pragma once

#include "src/wall/records.h"

#include <concepts>

namespace noctua::common {

template<typename F, typename T>
concept wall_writer_t = requires(F wall_writer, T data) {
  { wall_writer.store(data) } -> std::same_as<common::fibers::task_t<void>>;
};

} // namespace noctua::common
