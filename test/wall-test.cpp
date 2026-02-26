#include "src/wall/wall-header.h"
#include "src/wall/wall-writer.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <fstream>
#include <gtest/gtest.h>
#include <vector>

namespace noctua::wall {

class wall_test_t : public ::testing::Test {
protected:
  void SetUp() override {
    // Очищаем тестовый файл перед каждым тестом
    std::remove(test_file_path_.c_str());
  }

  void TearDown() override {
    // Удаляем тестовый файл после каждого теста
    std::remove(test_file_path_.c_str());
  }

  std::string test_file_path_ = "/tmp/test_wall.bin";

  std::vector<std::byte> read_file_content() {
    std::ifstream file(test_file_path_, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
      return {};
    }

    auto file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<std::byte> buffer(file_size);
    file.read(reinterpret_cast<char*>(buffer.data()), file_size);

    return buffer;
  }

  template<typename T>
  void write_to_writer(wall_writer_t& writer, const T& value) {
    boost::asio::io_context io_ctx;
    boost::asio::co_spawn(
            io_ctx,
            [&]() -> fibers::task_t<void> { co_await writer.store(&value, sizeof(value)); },
            boost::asio::detached);
    io_ctx.run();
  }
};

TEST_F(wall_test_t, store_raw_data_writes_correctly) {
  wall_writer_t writer(test_file_path_);
  const uint64_t test_value = 0xDEADBEEFCAFEBABE;

  write_to_writer(writer, test_value);

  auto content = read_file_content();
  ASSERT_EQ(content.size(), sizeof(test_value));

  uint64_t read_value;
  std::memcpy(&read_value, content.data(), sizeof(read_value));
  EXPECT_EQ(read_value, test_value);
}

TEST_F(wall_test_t, store_span_writes_correctly) {
  wall_writer_t writer(test_file_path_);
  std::vector<std::byte> test_data = {std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04}};

  boost::asio::io_context io_ctx;
  boost::asio::co_spawn(
          io_ctx,
          [&]() -> fibers::task_t<void> { co_await writer.store(std::span<const std::byte>(test_data)); },
          boost::asio::detached);
  io_ctx.run();

  auto content = read_file_content();
  ASSERT_EQ(content.size(), test_data.size());

  for (size_t i = 0; i < test_data.size(); ++i) {
    EXPECT_EQ(content[i], test_data[i]);
  }
}

TEST_F(wall_test_t, wall_header_store_writes_correct_structure) {
  wall_writer_t writer(test_file_path_);
  wall_header_t header;
  header.offset_id = 12345;
  header.topic_name = "test-topic";
  header.data_count = 42;

  boost::asio::io_context io_ctx;
  boost::asio::co_spawn(
          io_ctx, [&]() -> fibers::task_t<void> { co_await header.store(writer); }, boost::asio::detached);
  io_ctx.run();

  auto content = read_file_content();

  // Проверяем структуру:
  // magic (8 bytes) + offset_id (8 bytes) + topic_name_size (8 bytes) +
  // topic_name (10 bytes) + data_count (8 bytes) = 42 bytes
  ASSERT_EQ(content.size(), 42);

  // Проверяем magic
  uint64_t magic;
  std::memcpy(&magic, content.data(), sizeof(magic));
  EXPECT_EQ(magic, wall_header_t::magic);

  // Проверяем offset_id
  uint64_t offset_id;
  std::memcpy(&offset_id, content.data() + 8, sizeof(offset_id));
  EXPECT_EQ(offset_id, 12345);

  // Проверяем topic_name_size
  uint64_t topic_name_size;
  std::memcpy(&topic_name_size, content.data() + 16, sizeof(topic_name_size));
  EXPECT_EQ(topic_name_size, 10);

  // Проверяем topic_name
  std::string topic_name(reinterpret_cast<char*>(content.data() + 24), 10);
  EXPECT_EQ(topic_name, "test-topic");

  // Проверяем data_count
  uint64_t data_count;
  std::memcpy(&data_count, content.data() + 34, sizeof(data_count));
  EXPECT_EQ(data_count, 42);
}

TEST_F(wall_test_t, wall_data_store_writes_correct_structure) {
  wall_writer_t writer(test_file_path_);
  wall_data_t data;
  data.identifier = 99;
  data.is_push_operation = true;
  data.message = "hello world";

  boost::asio::io_context io_ctx;
  boost::asio::co_spawn(io_ctx, [&]() -> fibers::task_t<void> { co_await data.store(writer); }, boost::asio::detached);
  io_ctx.run();

  auto content = read_file_content();

  // Проверяем структуру:
  // magic (8 bytes) + identifier (2 bytes) + is_push_operation (1 byte) +
  // message_size (8 bytes) + message (11 bytes) = 30 bytes
  ASSERT_EQ(content.size(), 30);

  // Проверяем magic
  uint64_t magic;
  std::memcpy(&magic, content.data(), sizeof(magic));
  EXPECT_EQ(magic, wall_data_t::magic);

  // Проверяем identifier
  uint16_t identifier;
  std::memcpy(&identifier, content.data() + 8, sizeof(identifier));
  EXPECT_EQ(identifier, 99);

  // Проверяем is_push_operation
  bool is_push_operation;
  std::memcpy(&is_push_operation, content.data() + 10, sizeof(is_push_operation));
  EXPECT_TRUE(is_push_operation);

  // Проверяем message_size
  uint64_t message_size;
  std::memcpy(&message_size, content.data() + 11, sizeof(message_size));
  EXPECT_EQ(message_size, 11);

  // Проверяем message
  std::string message(reinterpret_cast<char*>(content.data() + 19), 11);
  EXPECT_EQ(message, "hello world");
}

TEST_F(wall_test_t, wall_footer_store_writes_correct_structure) {
  wall_writer_t writer(test_file_path_);
  wall_footer_t footer;
  footer.hash_sum = 0xABCDEF1234567890;

  boost::asio::io_context io_ctx;
  boost::asio::co_spawn(
          io_ctx, [&]() -> fibers::task_t<void> { co_await footer.store(writer); }, boost::asio::detached);
  io_ctx.run();

  auto content = read_file_content();

  // Проверяем структуру:
  // magic (8 bytes) + hash_sum (8 bytes) = 16 bytes
  ASSERT_EQ(content.size(), 16);

  // Проверяем magic
  uint64_t magic;
  std::memcpy(&magic, content.data(), sizeof(magic));
  EXPECT_EQ(magic, wall_footer_t::magic);

  // Проверяем hash_sum
  uint64_t hash_sum;
  std::memcpy(&hash_sum, content.data() + 8, sizeof(hash_sum));
  EXPECT_EQ(hash_sum, 0xABCDEF1234567890);
}

TEST_F(wall_test_t, multiple_writes_append_correctly) {
  wall_writer_t writer(test_file_path_);

  boost::asio::io_context io_ctx;
  boost::asio::co_spawn(
          io_ctx,
          [&]() -> fibers::task_t<void> {
            uint64_t first = 1;
            uint64_t second = 2;
            uint64_t third = 3;

            co_await writer.store(&first, sizeof(first));
            co_await writer.store(&second, sizeof(second));
            co_await writer.store(&third, sizeof(third));
          },
          boost::asio::detached);
  io_ctx.run();

  auto content = read_file_content();
  ASSERT_EQ(content.size(), 24);

  uint64_t first, second, third;
  std::memcpy(&first, content.data(), sizeof(first));
  std::memcpy(&second, content.data() + 8, sizeof(second));
  std::memcpy(&third, content.data() + 16, sizeof(third));

  EXPECT_EQ(first, 1);
  EXPECT_EQ(second, 2);
  EXPECT_EQ(third, 3);
}

TEST_F(wall_test_t, wall_header_with_empty_topic_name) {
  wall_writer_t writer(test_file_path_);
  wall_header_t header;
  header.offset_id = 0;
  header.topic_name = "";
  header.data_count = 0;

  boost::asio::io_context io_ctx;
  boost::asio::co_spawn(
          io_ctx, [&]() -> fibers::task_t<void> { co_await header.store(writer); }, boost::asio::detached);
  io_ctx.run();

  auto content = read_file_content();
  // magic + offset_id + topic_name_size + topic_name (empty) + data_count
  ASSERT_EQ(content.size(), 32);

  uint64_t topic_name_size;
  std::memcpy(&topic_name_size, content.data() + 16, sizeof(topic_name_size));
  EXPECT_EQ(topic_name_size, 0);
}

TEST_F(wall_test_t, wall_data_with_empty_message) {
  wall_writer_t writer(test_file_path_);
  wall_data_t data;
  data.identifier = 0;
  data.is_push_operation = false;
  data.message = "";

  boost::asio::io_context io_ctx;
  boost::asio::co_spawn(io_ctx, [&]() -> fibers::task_t<void> { co_await data.store(writer); }, boost::asio::detached);
  io_ctx.run();

  auto content = read_file_content();
  // magic + identifier + is_push_operation + message_size + message (empty)
  ASSERT_EQ(content.size(), 19);

  uint64_t message_size;
  std::memcpy(&message_size, content.data() + 11, sizeof(message_size));
  EXPECT_EQ(message_size, 0);
}

TEST_F(wall_test_t, wall_data_with_false_is_push_operation) {
  wall_writer_t writer(test_file_path_);
  wall_data_t data;
  data.identifier = 1;
  data.is_push_operation = false;
  data.message = "test";

  boost::asio::io_context io_ctx;
  boost::asio::co_spawn(io_ctx, [&]() -> fibers::task_t<void> { co_await data.store(writer); }, boost::asio::detached);
  io_ctx.run();

  auto content = read_file_content();

  bool is_push_operation;
  std::memcpy(&is_push_operation, content.data() + 10, sizeof(is_push_operation));
  EXPECT_FALSE(is_push_operation);
}

TEST_F(wall_test_t, writer_copy_constructor_is_deleted) {
  static_assert(!std::copy_constructible<wall_writer_t>);
}

TEST_F(wall_test_t, writer_move_constructor_is_deleted) {
  static_assert(!std::move_constructible<wall_writer_t>);
}

TEST_F(wall_test_t, writer_copy_assignment_is_deleted) {
  static_assert(!std::is_copy_assignable_v<wall_writer_t>);
}

TEST_F(wall_test_t, writer_move_assignment_is_deleted) {
  static_assert(!std::is_move_assignable_v<wall_writer_t>);
}

TEST_F(wall_test_t, wall_header_magic_is_zero) {
  static_assert(wall_header_t::magic == 0);
}

TEST_F(wall_test_t, wall_data_magic_is_one) {
  static_assert(wall_data_t::magic == 1);
}

TEST_F(wall_test_t, wall_footer_magic_is_two) {
  static_assert(wall_footer_t::magic == 2);
}

TEST_F(wall_test_t, complete_wall_structure) {
  wall_writer_t writer(test_file_path_);

  boost::asio::io_context io_ctx;
  boost::asio::co_spawn(
          io_ctx,
          [&]() -> fibers::task_t<void> {
            wall_header_t header;
            header.offset_id = 100;
            header.topic_name = "my-topic";
            header.data_count = 2;
            co_await header.store(writer);

            wall_data_t data1;
            data1.identifier = 0;
            data1.is_push_operation = true;
            data1.message = "first message";
            co_await data1.store(writer);

            wall_data_t data2;
            data2.identifier = 1;
            data2.is_push_operation = false;
            data2.message = "second message";
            co_await data2.store(writer);

            wall_footer_t footer;
            footer.hash_sum = 0xDEADBEEF;
            co_await footer.store(writer);
          },
          boost::asio::detached);
  io_ctx.run();

  auto content = read_file_content();

  // Проверяем размер
  // header: 8 + 8 + 8 + 8 + 8 = 40
  // data1: 8 + 2 + 1 + 8 + 13 = 32
  // data2: 8 + 2 + 1 + 8 + 14 = 33
  // footer: 8 + 8 = 16
  // total: 121
  ASSERT_EQ(content.size(), 121);

  // Проверяем все magic значения
  size_t offset = 0;

  uint64_t header_magic;
  std::memcpy(&header_magic, content.data() + offset, sizeof(header_magic));
  EXPECT_EQ(header_magic, wall_header_t::magic);

  offset = 40; // После header

  uint64_t data1_magic;
  std::memcpy(&data1_magic, content.data() + offset, sizeof(data1_magic));
  EXPECT_EQ(data1_magic, wall_data_t::magic);

  offset = 72; // После data1

  uint64_t data2_magic;
  std::memcpy(&data2_magic, content.data() + offset, sizeof(data2_magic));
  EXPECT_EQ(data2_magic, wall_data_t::magic);

  offset = 105; // После data2

  uint64_t footer_magic;
  std::memcpy(&footer_magic, content.data() + offset, sizeof(footer_magic));
  EXPECT_EQ(footer_magic, wall_footer_t::magic);
}

} // namespace noctua::wall
