
#include "common/fibers/task.h"
#include "common/fibers/sync-primitives/unique-mutex.h"
#include "common/fibers/sync-primitives/shared-mutex.h"

#include <atomic>
#include <cstdint>
#include <fmt/core.h>
#include <gtest/gtest.h>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>

namespace {
inline constexpr uint64_t TEST_COUNT = 100'000;
}

TEST(unique_mutex_test, basic_lock) {
  uint32_t real{0};
  std::atomic<uint32_t> counter{0};
  common::fibers::unique_mutex_t mutex;

  auto coro = [&]() -> common::fibers::task_t<void> {
    {
      for (uint64_t i = 0; i < TEST_COUNT; ++i) {
        ++counter;
        auto lock = co_await mutex.lock();
        ++real;
      }
      co_return;
    }
  };

  boost::asio::thread_pool pool(2);

  boost::asio::co_spawn(pool, coro(), boost::asio::detached);
  boost::asio::co_spawn(pool, coro(), boost::asio::detached);

  pool.join();

  EXPECT_EQ(real, counter.load());
}

TEST(shared_mutex_test, basic_lock) {
  uint32_t real{0};
  std::atomic<uint32_t> counter{0};
  common::fibers::shared_mutex_t mutex;

  auto coro = [&]() -> common::fibers::task_t<void> {
    {
      for (uint64_t i = 0; i < TEST_COUNT; ++i) {
        ++counter;
        auto lock = co_await mutex.lock();
        ++real;
      }
      co_return;
    }
  };

  boost::asio::thread_pool pool(2);

  boost::asio::co_spawn(pool, coro(), boost::asio::detached);
  boost::asio::co_spawn(pool, coro(), boost::asio::detached);

  pool.join();

  EXPECT_EQ(real, counter.load());
}

TEST(shared_mutex_test, mixed_lock) {
  uint32_t real{0};
  std::atomic<uint32_t> counter{0};
  common::fibers::shared_mutex_t mutex;

  auto write_coro = [&]() -> common::fibers::task_t<void> {
    {
      for (uint64_t i = 0; i < TEST_COUNT; ++i) {
        auto lock = co_await mutex.lock();
        ++real;
        ++counter;
      }
      co_return;
    }
  };
  auto read_coro = [&]() -> common::fibers::task_t<void> {
    {
      for (uint64_t i = 0; i < TEST_COUNT; ++i) {
        auto lock = co_await mutex.lock_shared();
        EXPECT_EQ(real, counter.load(std::memory_order_relaxed));
      }
      co_return;
    }
  };

  boost::asio::thread_pool pool(4);

  boost::asio::co_spawn(pool, write_coro(), boost::asio::detached);
  boost::asio::co_spawn(pool, read_coro(), boost::asio::detached);
  boost::asio::co_spawn(pool, read_coro(), boost::asio::detached);
  boost::asio::co_spawn(pool, read_coro(), boost::asio::detached);

  pool.join();

  EXPECT_EQ(real, counter.load());
}
