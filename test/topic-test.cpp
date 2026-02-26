#include "src/common/request.h"
#include "src/topic.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <gtest/gtest.h>

namespace noctua {

struct test_hash_t {
  std::size_t operator()(const common::request_t& request) const noexcept {
    std::size_t h = 0;
    std::string_view topic_name = request.topic_name();
    for (char c : topic_name) {
      h ^= std::hash<char>{}(c);
    }
    return h;
  }
};

class topic_test_t : public ::testing::Test {
protected:
  void SetUp() override {
    std::remove("/tmp/test_topic.wall");
  }

  void TearDown() override {
    std::remove("/tmp/test_topic.wall");
  }
};

TEST_F(topic_test_t, constructor_creates_topic_with_partitions) {
  topic_t<test_hash_t> topic{3, "/tmp/test_topic.wall", "test-topic"};
}

TEST_F(topic_test_t, push_with_valid_partition_id_puts_in_correct_partition) {
  topic_t<test_hash_t> topic{3, "/tmp/test_topic.wall", "test-topic"};

  std::array<std::byte, 1024> request_buffer;
  uint16_t topic_name_len = 9;
  uint16_t partition_id = 1;
  uint16_t message_len = 12;
  const char* message = "hello world";

  std::memcpy(request_buffer.data(), &topic_name_len, sizeof(topic_name_len));
  std::memcpy(request_buffer.data() + 2, &partition_id, sizeof(partition_id));
  std::memcpy(request_buffer.data() + 4, &message_len, sizeof(message_len));
  std::memcpy(request_buffer.data() + 6, message, message_len);

  const common::request_t& request = *reinterpret_cast<const common::request_t*>(request_buffer.data());

  boost::asio::io_context io_ctx;
  boost::asio::co_spawn(
          io_ctx,
          [&]() -> fibers::task_t<void> {
            co_await topic.push(request);
            co_return;
          },
          boost::asio::detached);
  io_ctx.run();
}

TEST_F(topic_test_t, push_with_invalid_partition_id_uses_hash) {
  topic_t<test_hash_t> topic{3, "/tmp/test_topic.wall", "test-topic"};

  std::array<std::byte, 1024> request_buffer;
  uint16_t topic_name_len = 9;
  uint16_t partition_id = common::INVALID_TOPIC_ID;
  uint16_t message_len = 5;
  const char* message = "test";

  std::memcpy(request_buffer.data(), &topic_name_len, sizeof(topic_name_len));
  std::memcpy(request_buffer.data() + 2, &partition_id, sizeof(partition_id));
  std::memcpy(request_buffer.data() + 4, &message_len, sizeof(message_len));
  std::memcpy(request_buffer.data() + 6, message, message_len);

  const common::request_t& request = *reinterpret_cast<const common::request_t*>(request_buffer.data());

  boost::asio::io_context io_ctx;
  boost::asio::co_spawn(
          io_ctx,
          [&]() -> fibers::task_t<void> {
            co_await topic.push(request);
            co_return;
          },
          boost::asio::detached);
  io_ctx.run();
}

TEST_F(topic_test_t, push_multiple_messages_goes_to_correct_partitions) {
  topic_t<test_hash_t> topic{3, "/tmp/test_topic.wall", "test-topic"};

  {
    std::array<std::byte, 1024> request_buffer;
    uint16_t topic_name_len = 9;
    uint16_t partition_id = 1;
    uint16_t message_len = 4;
    const char* message = "msg1";

    std::memcpy(request_buffer.data(), &topic_name_len, sizeof(topic_name_len));
    std::memcpy(request_buffer.data() + 2, &partition_id, sizeof(partition_id));
    std::memcpy(request_buffer.data() + 4, &message_len, sizeof(message_len));
    std::memcpy(request_buffer.data() + 6, message, message_len);

    const common::request_t& request = *reinterpret_cast<const common::request_t*>(request_buffer.data());

    boost::asio::io_context io_ctx;
    boost::asio::co_spawn(
            io_ctx,
            [&]() -> fibers::task_t<void> {
              co_await topic.push(request);
              co_return;
            },
            boost::asio::detached);
    io_ctx.run();
  }

  {
    std::array<std::byte, 1024> request_buffer;
    uint16_t topic_name_len = 9;
    uint16_t partition_id = 2;
    uint16_t message_len = 4;
    const char* message = "msg2";

    std::memcpy(request_buffer.data(), &topic_name_len, sizeof(topic_name_len));
    std::memcpy(request_buffer.data() + 2, &partition_id, sizeof(partition_id));
    std::memcpy(request_buffer.data() + 4, &message_len, sizeof(message_len));
    std::memcpy(request_buffer.data() + 6, message, message_len);

    const common::request_t& request = *reinterpret_cast<const common::request_t*>(request_buffer.data());

    boost::asio::io_context io_ctx;
    boost::asio::co_spawn(
            io_ctx,
            [&]() -> fibers::task_t<void> {
              co_await topic.push(request);
              co_return;
            },
            boost::asio::detached);
    io_ctx.run();
  }
}

static_assert(!std::copy_constructible<topic_t<test_hash_t>>);
static_assert(!std::move_constructible<topic_t<test_hash_t>>);
static_assert(!std::is_copy_assignable_v<topic_t<test_hash_t>>);
static_assert(!std::is_move_assignable_v<topic_t<test_hash_t>>);
static_assert(std::is_destructible_v<topic_t<test_hash_t>>);

} // namespace noctua
