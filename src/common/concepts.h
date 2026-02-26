#pragma once

#include "common/fibers/task.h"

#include <span>
#include <concepts>
#include <cstddef>
#include <string_view>
#include <type_traits>

namespace noctua::common {

template<typename F>
concept wall_writer_t = requires(F wall_writer, const void* raw_data, size_t data_size, std::span<const std::byte> data) {
  requires std::is_constructible_v<F, std::string_view>;
  { wall_writer.store(raw_data, data_size) } -> std::same_as<fibers::task_t<void>>;
  { wall_writer.store(data) } -> std::same_as<fibers::task_t<void>>;
};

} // namespace noctua::common
