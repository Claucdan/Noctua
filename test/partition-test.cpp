
#include "src/partition.h"

#include <atomic>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>
#include <gtest/gtest.h>

namespace noctua {

TEST(partition_test_t, constructor_creates_empty_partition) {
  partition_t partition;

  boost::asio::io_context io_ctx;
  boost::asio::co_spawn(
          io_ctx,
          [&]() -> fibers::task_t<void> {
            auto lock = co_await partition.read_lock();
            EXPECT_TRUE(partition.empty());
            EXPECT_EQ(partition.size(), 0);
            EXPECT_EQ(partition.begin(), partition.end());
            EXPECT_EQ(partition.cbegin(), partition.cend());
            co_return;
          },
          boost::asio::detached);
  io_ctx.run();
}

TEST(partition_test_t, push_adds_message_to_front) {
  partition_t partition;
  const std::string_view TEST_MESSAGE = "test message";

  boost::asio::io_context io_ctx;
  boost::asio::co_spawn(
          io_ctx,
          [&]() -> fibers::task_t<void> {
            auto lock = co_await partition.write_lock();
            auto it = partition.push(TEST_MESSAGE);

            EXPECT_FALSE(partition.empty());
            EXPECT_EQ(partition.size(), 1);
            EXPECT_EQ(it, partition.begin());
            EXPECT_NE(partition.begin(), partition.end());
            EXPECT_EQ(it->get_data(), TEST_MESSAGE);
            co_return;
          },
          boost::asio::detached);
  io_ctx.run();
}

TEST(partition_test_t, push_multiple_messages) {
  partition_t partition;
  const std::string_view FIRST_MESSAGE = "first message";
  const std::string_view SECOND_MESSAGE = "second message";
  const std::string_view THIRD_MESSAGE = "third message";

  boost::asio::io_context io_ctx;
  boost::asio::co_spawn(
          io_ctx,
          [&]() -> fibers::task_t<void> {
            auto lock = co_await partition.write_lock();
            partition.push(FIRST_MESSAGE);
            partition.push(SECOND_MESSAGE);
            partition.push(THIRD_MESSAGE);

            EXPECT_EQ(partition.size(), 3);
            EXPECT_EQ((*partition.begin()).get_data(), FIRST_MESSAGE);

            auto current = partition.begin();
            EXPECT_EQ((*current).get_data(), FIRST_MESSAGE);
            ++current;
            EXPECT_EQ((*current).get_data(), SECOND_MESSAGE);
            ++current;
            EXPECT_EQ((*current).get_data(), THIRD_MESSAGE);
            ++current;
            EXPECT_EQ(current, partition.end());
            co_return;
          },
          boost::asio::detached);
  io_ctx.run();
}

TEST(partition_test_t, pop_removes_front_message) {
  partition_t partition;
  const std::string_view FIRST_MESSAGE = "first message";
  const std::string_view SECOND_MESSAGE = "second message";

  boost::asio::io_context io_ctx;
  boost::asio::co_spawn(
          io_ctx,
          [&]() -> fibers::task_t<void> {
            auto lock = co_await partition.write_lock();
            partition.push(FIRST_MESSAGE);
            partition.push(SECOND_MESSAGE);
            EXPECT_EQ(partition.size(), 2);

            partition.pop();
            EXPECT_EQ(partition.size(), 1);
            EXPECT_EQ((*partition.begin()).get_data(), SECOND_MESSAGE);

            partition.pop();
            EXPECT_TRUE(partition.empty());
            EXPECT_EQ(partition.size(), 0);
            EXPECT_EQ(partition.begin(), partition.end());
            co_return;
          },
          boost::asio::detached);
  io_ctx.run();
}

TEST(partition_test_t, pop_on_empty_partition_does_nothing) {
  partition_t partition;

  boost::asio::io_context io_ctx;
  boost::asio::co_spawn(
          io_ctx,
          [&]() -> fibers::task_t<void> {
            auto lock = co_await partition.write_lock();
            EXPECT_TRUE(partition.empty());

            partition.pop();

            EXPECT_TRUE(partition.empty());
            EXPECT_EQ(partition.size(), 0);
            co_return;
          },
          boost::asio::detached);
  io_ctx.run();
}

TEST(partition_test_t, iterator_dereference_returns_message) {
  partition_t partition;
  const std::string_view TEST_MESSAGE = "test message";

  boost::asio::io_context io_ctx;
  boost::asio::co_spawn(
          io_ctx,
          [&]() -> fibers::task_t<void> {
            auto lock = co_await partition.write_lock();
            partition.push(TEST_MESSAGE);
            auto it = partition.begin();

            messages::message_t& message = *it;
            EXPECT_EQ(message.get_data(), TEST_MESSAGE);
            co_return;
          },
          boost::asio::detached);
  io_ctx.run();
}

TEST(partition_test_t, iterator_arrow_operator_works) {
  partition_t partition;
  const std::string_view TEST_MESSAGE = "test message";

  boost::asio::io_context io_ctx;
  boost::asio::co_spawn(
          io_ctx,
          [&]() -> fibers::task_t<void> {
            auto lock = co_await partition.write_lock();
            partition.push(TEST_MESSAGE);
            auto it = partition.begin();

            EXPECT_EQ(it->get_data(), TEST_MESSAGE);
            co_return;
          },
          boost::asio::detached);
  io_ctx.run();
}

TEST(partition_test_t, iterator_pre_increment_works) {
  partition_t partition;
  const std::string_view FIRST_MESSAGE = "first message";
  const std::string_view SECOND_MESSAGE = "second message";

  boost::asio::io_context io_ctx;
  boost::asio::co_spawn(
          io_ctx,
          [&]() -> fibers::task_t<void> {
            auto lock = co_await partition.write_lock();
            partition.push(FIRST_MESSAGE);
            partition.push(SECOND_MESSAGE);

            auto it = partition.begin();
            EXPECT_EQ((*it).get_data(), FIRST_MESSAGE);

            auto& it_ref = ++it;
            EXPECT_EQ(&it_ref, &it);
            EXPECT_EQ((*it).get_data(), SECOND_MESSAGE);
            co_return;
          },
          boost::asio::detached);
  io_ctx.run();
}

TEST(partition_test_t, iterator_post_increment_works) {
  partition_t partition;
  const std::string_view FIRST_MESSAGE = "first message";
  const std::string_view SECOND_MESSAGE = "second message";

  boost::asio::io_context io_ctx;
  boost::asio::co_spawn(
          io_ctx,
          [&]() -> fibers::task_t<void> {
            auto lock = co_await partition.write_lock();
            partition.push(FIRST_MESSAGE);
            partition.push(SECOND_MESSAGE);

            auto it = partition.begin();
            EXPECT_EQ((*it).get_data(), FIRST_MESSAGE);

            auto old_it = it++;
            EXPECT_EQ((*old_it).get_data(), FIRST_MESSAGE);
            EXPECT_EQ((*it).get_data(), SECOND_MESSAGE);
            co_return;
          },
          boost::asio::detached);
  io_ctx.run();
}

TEST(partition_test_t, const_iterator_can_be_created_from_cbegin) {
  partition_t partition;
  const std::string_view TEST_MESSAGE = "test message";

  {
    boost::asio::io_context io_ctx;
    boost::asio::co_spawn(
            io_ctx,
            [&]() -> fibers::task_t<void> {
              auto lock = co_await partition.write_lock();
              partition.push(TEST_MESSAGE);
              co_return;
            },
            boost::asio::detached);
    io_ctx.run();
  }

  boost::asio::io_context io_ctx2;
  boost::asio::co_spawn(
          io_ctx2,
          [&]() -> fibers::task_t<void> {
            auto lock = co_await partition.read_lock();
            auto cit = partition.cbegin();
            EXPECT_EQ((*cit).get_data(), TEST_MESSAGE);
            EXPECT_EQ(cit, partition.cbegin());
            EXPECT_NE(cit, partition.cend());
            co_return;
          },
          boost::asio::detached);
  io_ctx2.run();
}

TEST(partition_test_t, const_iterator_traversal_works) {
  partition_t partition;
  const std::string_view FIRST_MESSAGE = "first message";
  const std::string_view SECOND_MESSAGE = "second message";

  {
    boost::asio::io_context io_ctx;
    boost::asio::co_spawn(
            io_ctx,
            [&]() -> fibers::task_t<void> {
              auto lock = co_await partition.write_lock();
              partition.push(FIRST_MESSAGE);
              partition.push(SECOND_MESSAGE);
              co_return;
            },
            boost::asio::detached);
    io_ctx.run();
  }

  boost::asio::io_context io_ctx2;
  boost::asio::co_spawn(
          io_ctx2,
          [&]() -> fibers::task_t<void> {
            auto lock = co_await partition.read_lock();
            auto cit = partition.cbegin();
            EXPECT_EQ((*cit).get_data(), FIRST_MESSAGE);
            ++cit;
            EXPECT_EQ((*cit).get_data(), SECOND_MESSAGE);
            ++cit;
            EXPECT_EQ(cit, partition.cend());
            co_return;
          },
          boost::asio::detached);
  io_ctx2.run();
}

TEST(partition_test_t, non_const_iterator_converts_to_const_iterator) {
  partition_t partition;
  const std::string_view TEST_MESSAGE = "test message";

  {
    boost::asio::io_context io_ctx;
    boost::asio::co_spawn(
            io_ctx,
            [&]() -> fibers::task_t<void> {
              auto lock = co_await partition.write_lock();
              partition.push(TEST_MESSAGE);
              co_return;
            },
            boost::asio::detached);
    io_ctx.run();
  }

  boost::asio::io_context io_ctx2;
  boost::asio::co_spawn(
          io_ctx2,
          [&]() -> fibers::task_t<void> {
            auto lock = co_await partition.write_lock();
            partition_t::iterator it = partition.begin();
            partition_t::const_iterator cit = it;

            EXPECT_EQ(cit, partition.cbegin());
            EXPECT_EQ((*cit).get_data(), TEST_MESSAGE);
            co_return;
          },
          boost::asio::detached);
  io_ctx2.run();
}

TEST(partition_test_t, iterators_equality_comparison_works) {
  partition_t partition;
  const std::string_view TEST_MESSAGE = "test message";

  {
    boost::asio::io_context io_ctx;
    boost::asio::co_spawn(
            io_ctx,
            [&]() -> fibers::task_t<void> {
              auto lock = co_await partition.write_lock();
              partition.push(TEST_MESSAGE);
              co_return;
            },
            boost::asio::detached);
    io_ctx.run();
  }

  {
    boost::asio::io_context io_ctx2;
    boost::asio::co_spawn(
            io_ctx2,
            [&]() -> fibers::task_t<void> {
              auto lock = co_await partition.write_lock();
              auto it1 = partition.begin();
              auto it2 = partition.begin();
              auto it3 = partition.end();

              EXPECT_EQ(it1, it2);
              EXPECT_NE(it1, it3);
              co_return;
            },
            boost::asio::detached);
    io_ctx2.run();
  }

  {
    boost::asio::io_context io_ctx3;
    boost::asio::co_spawn(
            io_ctx3,
            [&]() -> fibers::task_t<void> {
              auto lock = co_await partition.read_lock();
              auto cit1 = partition.cbegin();
              auto cit2 = partition.cbegin();

              EXPECT_EQ(cit1, cit2);
              EXPECT_NE(cit1, partition.cend());
              co_return;
            },
            boost::asio::detached);
    io_ctx3.run();
  }
}

TEST(partition_test_t, iterators_cross_comparison_works) {
  partition_t partition;
  const std::string_view TEST_MESSAGE = "test message";

  {
    boost::asio::io_context io_ctx;
    boost::asio::co_spawn(
            io_ctx,
            [&]() -> fibers::task_t<void> {
              auto lock = co_await partition.write_lock();
              partition.push(TEST_MESSAGE);
              co_return;
            },
            boost::asio::detached);
    io_ctx.run();
  }

  boost::asio::io_context io_ctx2;
  boost::asio::co_spawn(
          io_ctx2,
          [&]() -> fibers::task_t<void> {
            auto lock = co_await partition.write_lock();
            auto it = partition.begin();
            auto cit = partition_t::const_iterator{it};
            EXPECT_EQ(it, cit);
            EXPECT_EQ(cit, it);
            co_return;
          },
          boost::asio::detached);
  io_ctx2.run();
}

TEST(partition_test_t, front_returns_iterator_to_begin) {
  partition_t partition;
  const std::string_view FIRST_MESSAGE = "first message";
  const std::string_view SECOND_MESSAGE = "second message";

  {
    boost::asio::io_context io_ctx;
    boost::asio::co_spawn(
            io_ctx,
            [&]() -> fibers::task_t<void> {
              auto lock = co_await partition.write_lock();
              partition.push(FIRST_MESSAGE);
              partition.push(SECOND_MESSAGE);

              EXPECT_EQ(partition.front(), partition.begin());
              EXPECT_EQ((*partition.front()).get_data(), FIRST_MESSAGE);
              co_return;
            },
            boost::asio::detached);
    io_ctx.run();
  }

  {
    boost::asio::io_context io_ctx2;
    boost::asio::co_spawn(
            io_ctx2,
            [&]() -> fibers::task_t<void> {
              auto lock = co_await partition.read_lock();
              const auto& const_partition = partition;
              EXPECT_EQ(const_partition.front(), const_partition.cbegin());
              EXPECT_EQ((*const_partition.front()).get_data(), FIRST_MESSAGE);
              co_return;
            },
            boost::asio::detached);
    io_ctx2.run();
  }
}

TEST(partition_test_t, back_returns_iterator_to_begin) {
  partition_t partition;
  const std::string_view FIRST_MESSAGE = "first message";
  const std::string_view SECOND_MESSAGE = "second message";

  {
    boost::asio::io_context io_ctx;
    boost::asio::co_spawn(
            io_ctx,
            [&]() -> fibers::task_t<void> {
              auto lock = co_await partition.write_lock();
              partition.push(FIRST_MESSAGE);
              partition.push(SECOND_MESSAGE);

              EXPECT_EQ(partition.back(), partition.begin());
              EXPECT_EQ((*partition.back()).get_data(), FIRST_MESSAGE);
              co_return;
            },
            boost::asio::detached);
    io_ctx.run();
  }

  {
    boost::asio::io_context io_ctx2;
    boost::asio::co_spawn(
            io_ctx2,
            [&]() -> fibers::task_t<void> {
              auto lock = co_await partition.read_lock();
              const auto& const_partition = partition;
              EXPECT_EQ(const_partition.back(), const_partition.cbegin());
              EXPECT_EQ((*const_partition.back()).get_data(), FIRST_MESSAGE);
              co_return;
            },
            boost::asio::detached);
    io_ctx2.run();
  }
}

TEST(partition_test_t, range_based_for_loop_works_with_iterator) {
  partition_t partition;
  const std::string_view MESSAGES[] = {"first", "second", "third"};
  constexpr size_t MESSAGE_COUNT = 3;

  {
    boost::asio::io_context io_ctx;
    boost::asio::co_spawn(
            io_ctx,
            [&]() -> fibers::task_t<void> {
              auto lock = co_await partition.write_lock();
              for (const auto& msg : MESSAGES) {
                partition.push(msg);
              }
              co_return;
            },
            boost::asio::detached);
    io_ctx.run();
  }

  {
    boost::asio::io_context io_ctx2;
    boost::asio::co_spawn(
            io_ctx2,
            [&]() -> fibers::task_t<void> {
              auto lock = co_await partition.read_lock();
              size_t INDEX = 0;
              for (const auto& msg : partition) {
                EXPECT_EQ(msg.get_data(), MESSAGES[INDEX]);
                ++INDEX;
              }
              EXPECT_EQ(INDEX, MESSAGE_COUNT);
              co_return;
            },
            boost::asio::detached);
    io_ctx2.run();
  }
}

TEST(partition_test_t, iterator_concepts_are_satisfied) {
  using iterator = partition_t::iterator;
  using const_iterator = partition_t::const_iterator;

  static_assert(std::forward_iterator<iterator>);
  static_assert(std::forward_iterator<const_iterator>);
  static_assert(std::equality_comparable<iterator>);
  static_assert(std::equality_comparable<const_iterator>);
  static_assert(std::equality_comparable_with<iterator, const_iterator>);
}

TEST(partition_test_t, large_number_of_messages) {
  partition_t partition;
  constexpr size_t MESSAGE_COUNT = 1000;
  const std::string_view BASE_MESSAGE = "message ";

  {
    boost::asio::io_context io_ctx;
    boost::asio::co_spawn(
            io_ctx,
            [&]() -> fibers::task_t<void> {
              auto lock = co_await partition.write_lock();
              for (size_t i = 0; i < MESSAGE_COUNT; ++i) {
                std::string msg = std::string(BASE_MESSAGE) + std::to_string(i);
                partition.push(msg);
              }

              EXPECT_EQ(partition.size(), MESSAGE_COUNT);
              co_return;
            },
            boost::asio::detached);
    io_ctx.run();
  }

  {
    boost::asio::io_context io_ctx2;
    boost::asio::co_spawn(
            io_ctx2,
            [&]() -> fibers::task_t<void> {
              auto lock = co_await partition.read_lock();
              size_t COUNT = 0;
              for ([[maybe_unused]] const auto& msg : partition) {
                ++COUNT;
              }
              EXPECT_EQ(COUNT, MESSAGE_COUNT);
              co_return;
            },
            boost::asio::detached);
    io_ctx2.run();
  }
}

TEST(partition_test_t, empty_partition_iterators_are_equal) {
  partition_t partition;

  {
    boost::asio::io_context io_ctx;
    boost::asio::co_spawn(
            io_ctx,
            [&]() -> fibers::task_t<void> {
              auto lock = co_await partition.write_lock();
              EXPECT_EQ(partition.begin(), partition.end());
              co_return;
            },
            boost::asio::detached);
    io_ctx.run();
  }

  {
    boost::asio::io_context io_ctx2;
    boost::asio::co_spawn(
            io_ctx2,
            [&]() -> fibers::task_t<void> {
              auto lock = co_await partition.read_lock();
              EXPECT_EQ(partition.cbegin(), partition.cend());
              co_return;
            },
            boost::asio::detached);
    io_ctx2.run();
  }
}

TEST(partition_test_t, copy_constructor_is_deleted) {
  static_assert(!std::copy_constructible<partition_t>);
}

TEST(partition_test_t, move_constructor_is_deleted) {
  static_assert(!std::move_constructible<partition_t>);
}

TEST(partition_test_t, copy_assignment_is_deleted) {
  static_assert(!std::is_copy_assignable_v<partition_t>);
}

TEST(partition_test_t, move_assignment_is_deleted) {
  static_assert(!std::is_move_assignable_v<partition_t>);
}

TEST(partition_test_t, destructor_is_default) {
  static_assert(std::is_destructible_v<partition_t>);
}

namespace {

inline constexpr uint64_t TEST_COUNT = 100'000;

} // namespace

TEST(partition_concurrent_test, write_lock_exclusive_access) {
  partition_t partition;
  uint32_t real{0};
  std::atomic<uint32_t> counter{0};

  auto coro = [&]() -> fibers::task_t<void> {
    {
      for (uint64_t i = 0; i < TEST_COUNT; ++i) {
        ++counter;
        auto lock = co_await partition.write_lock();
        ++real;
        partition.push("test");
      }
      co_return;
    }
  };

  boost::asio::thread_pool pool(2);

  boost::asio::co_spawn(pool, coro(), boost::asio::detached);
  boost::asio::co_spawn(pool, coro(), boost::asio::detached);

  pool.join();

  {
    boost::asio::io_context io_ctx;
    boost::asio::co_spawn(
            io_ctx,
            [&]() -> fibers::task_t<void> {
              auto lock = co_await partition.read_lock();
              EXPECT_EQ(real, counter.load());
              EXPECT_EQ(partition.size(), real);
              co_return;
            },
            boost::asio::detached);
    io_ctx.run();
  }
}

TEST(partition_concurrent_test, read_lock_concurrent_access) {
  partition_t partition;
  {
    boost::asio::io_context io_ctx;
    boost::asio::co_spawn(
            io_ctx,
            [&]() -> fibers::task_t<void> {
              auto lock = co_await partition.write_lock();
              for (uint64_t i = 0; i < TEST_COUNT; ++i) {
                partition.push("test");
              }
              co_return;
            },
            boost::asio::detached);
    io_ctx.run();
  }

  std::atomic<uint64_t> read_count{0};

  auto read_coro = [&]() -> fibers::task_t<void> {
    {
      for (uint64_t i = 0; i < TEST_COUNT; ++i) {
        auto lock = co_await partition.read_lock();
        ++read_count;
      }
      co_return;
    }
  };

  boost::asio::thread_pool pool(4);

  boost::asio::co_spawn(pool, read_coro(), boost::asio::detached);
  boost::asio::co_spawn(pool, read_coro(), boost::asio::detached);
  boost::asio::co_spawn(pool, read_coro(), boost::asio::detached);
  boost::asio::co_spawn(pool, read_coro(), boost::asio::detached);

  pool.join();

  EXPECT_EQ(read_count.load(), 4 * TEST_COUNT);
}

TEST(partition_concurrent_test, mixed_lock_operations) {
  partition_t partition;
  uint32_t real{0};
  std::atomic<uint32_t> counter{0};

  auto write_coro = [&]() -> fibers::task_t<void> {
    {
      for (uint64_t i = 0; i < TEST_COUNT; ++i) {
        auto lock = co_await partition.write_lock();
        ++real;
        ++counter;
        partition.push("test");
      }
      co_return;
    }
  };

  auto read_coro = [&]() -> fibers::task_t<void> {
    {
      for (uint64_t i = 0; i < TEST_COUNT; ++i) {
        auto lock = co_await partition.read_lock();
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

  {
    boost::asio::io_context io_ctx;
    boost::asio::co_spawn(
            io_ctx,
            [&]() -> fibers::task_t<void> {
              auto lock = co_await partition.read_lock();
              EXPECT_EQ(real, counter.load());
              EXPECT_EQ(partition.size(), real);
              co_return;
            },
            boost::asio::detached);
    io_ctx.run();
  }
}

TEST(partition_concurrent_test, concurrent_push_and_read) {
  partition_t partition;
  const std::string_view TEST_MESSAGE = "test message";
  std::atomic<uint32_t> push_count{0};
  std::atomic<uint32_t> read_count{0};

  constexpr uint64_t SMALL_TEST_COUNT = 1000;

  auto push_coro = [&]() -> fibers::task_t<void> {
    {
      for (uint64_t i = 0; i < SMALL_TEST_COUNT; ++i) {
        auto lock = co_await partition.write_lock();
        partition.push(TEST_MESSAGE);
        ++push_count;
      }
      co_return;
    }
  };

  auto read_coro = [&]() -> fibers::task_t<void> {
    {
      for (uint64_t i = 0; i < SMALL_TEST_COUNT; ++i) {
        auto lock = co_await partition.read_lock();
        ++read_count;
        for ([[maybe_unused]] const auto& msg : partition) {
          EXPECT_EQ(msg.get_data(), TEST_MESSAGE);
        }
      }
      co_return;
    }
  };

  boost::asio::thread_pool pool(4);

  boost::asio::co_spawn(pool, push_coro(), boost::asio::detached);
  boost::asio::co_spawn(pool, push_coro(), boost::asio::detached);
  boost::asio::co_spawn(pool, read_coro(), boost::asio::detached);
  boost::asio::co_spawn(pool, read_coro(), boost::asio::detached);

  pool.join();

  {
    boost::asio::io_context io_ctx;
    boost::asio::co_spawn(
            io_ctx,
            [&]() -> fibers::task_t<void> {
              auto lock = co_await partition.read_lock();
              EXPECT_EQ(push_count.load(), 2 * SMALL_TEST_COUNT);
              EXPECT_EQ(partition.size(), 2 * SMALL_TEST_COUNT);
              co_return;
            },
            boost::asio::detached);
    io_ctx.run();
  }
}

TEST(partition_concurrent_test, concurrent_push_pop) {
  partition_t partition;
  const std::string_view TEST_MESSAGE = "test message";
  std::atomic<uint32_t> push_count{0};
  std::atomic<uint32_t> pop_count{0};

  auto push_coro = [&]() -> fibers::task_t<void> {
    {
      for (uint64_t i = 0; i < TEST_COUNT; ++i) {
        auto lock = co_await partition.write_lock();
        partition.push(TEST_MESSAGE);
        ++push_count;
      }
      co_return;
    }
  };

  auto pop_coro = [&]() -> fibers::task_t<void> {
    {
      for (uint64_t i = 0; i < TEST_COUNT; ++i) {
        auto lock = co_await partition.write_lock();
        if (!partition.empty()) {
          partition.pop();
          ++pop_count;
        }
      }
      co_return;
    }
  };

  boost::asio::thread_pool pool(4);

  boost::asio::co_spawn(pool, push_coro(), boost::asio::detached);
  boost::asio::co_spawn(pool, push_coro(), boost::asio::detached);
  boost::asio::co_spawn(pool, pop_coro(), boost::asio::detached);
  boost::asio::co_spawn(pool, pop_coro(), boost::asio::detached);

  pool.join();

  {
    boost::asio::io_context io_ctx;
    boost::asio::co_spawn(
            io_ctx,
            [&]() -> fibers::task_t<void> {
              auto lock = co_await partition.read_lock();
              EXPECT_EQ(push_count.load(), 2 * TEST_COUNT);
              EXPECT_EQ(partition.size() + pop_count.load(), 2 * TEST_COUNT);
              co_return;
            },
            boost::asio::detached);
    io_ctx.run();
  }
}

} // namespace noctua
