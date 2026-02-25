#pragma once

#include <cstdlib>
#include <fmt/core.h>

namespace noctua {

#define kassert(expr, msg, ...)                                                                                        \
  {                                                                                                                    \
    if (!(expr)) [[unlikely]] {                                                                                        \
      fmt::print(stderr, "[FATAL][{}:{}] ", __FILE__, __LINE__, __VA_ARGS__);                                          \
      fmt::println(stderr, (msg), __VA_ARGS__);                                                                        \
      std::abort();                                                                                                    \
    }                                                                                                                  \
  }

#define kassert_cmp(left, right, op)                                                                                   \
  {                                                                                                                    \
    if (!((left)op(right))) [[unlikely]] {                                                                             \
      fmt::print(stderr, "[FATAL][{}:{}] Assart comparing ", __FILE__, __LINE__);                                      \
      fmt::println(stderr, "{} {} {} !!!", #left, #op, #right);                                                        \
      std::abort();                                                                                                    \
    }                                                                                                                  \
  }

#define kassert_eq(left, right) kassert_cmp((left), (right), ==)
#define kassert_ne(left, right) kassert_cmp((left), (right), !=)
#define kassert_ge(left, right) kassert_cmp((left), (right), >=)
#define kassert_gt(left, right) kassert_cmp((left), (right), >)
#define kassert_le(left, right) kassert_cmp((left), (right), <=)
#define kassert_lt(left, right) kassert_cmp((left), (right), <)

} // namespace noctua
