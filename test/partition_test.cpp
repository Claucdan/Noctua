#include "src/partition.h"
#include "src/common/message.h"

#include <boost/cobalt.hpp>
#include <chrono>

#include <gtest/gtest.h>

namespace {

using message_t = noctua::common::message_t;
using message_ptr_t = message_t::message_ptr_t;

} // namespace

namespace noctua {

TEST(PartitionTest, SingleReaderCoroutine) {
  partition_t partition;
  message_ptr_t msg = message_t::create_message("data");

  partition.push(std::move(msg));

  auto reader_task = [&]() -> boost::cobalt::task<void> {
    auto lk = co_await partition.read_lock();
    EXPECT_EQ("data", partition.front().get_data());
    co_return;
  };

  boost::cobalt::run(reader_task());
}

TEST(PartitionTest, PushAndFront) {
  partition_t partition;

  partition.push(message_t::create_message("first"));
  partition.push(message_t::create_message("second"));
  partition.push(message_t::create_message("third"));

  EXPECT_EQ("first", partition.front().get_data());
}

TEST(PartitionTest, PushAndBack) {
  partition_t partition;

  partition.push(message_t::create_message("first"));
  partition.push(message_t::create_message("second"));
  partition.push(message_t::create_message("third"));

  EXPECT_EQ("third", partition.back().get_data());
}

TEST(PartitionTest, PopRemovesFrontElement) {
  partition_t partition;

  partition.push(message_t::create_message("first"));
  partition.push(message_t::create_message("second"));

  EXPECT_EQ("first", partition.front().get_data());

  partition.pop();

  EXPECT_EQ("second", partition.front().get_data());
}

TEST(PartitionTest, MultiplePops) {
  partition_t partition;

  partition.push(message_t::create_message("first"));
  partition.push(message_t::create_message("second"));
  partition.push(message_t::create_message("third"));

  partition.pop();
  EXPECT_EQ("second", partition.front().get_data());

  partition.pop();
  EXPECT_EQ("third", partition.front().get_data());
}

TEST(PartitionTest, WriteLockCoroutine) {
  partition_t partition;
  message_ptr_t msg = message_t::create_message("data");

  auto writer_task = [&]() -> boost::cobalt::task<void> {
    auto lk = co_await partition.write_lock();
    partition.push(std::move(msg));
    EXPECT_EQ("data", partition.front().get_data());
    co_return;
  };

  boost::cobalt::run(writer_task());
}

TEST(PartitionTest, ReadWriteWithLocks) {
  partition_t partition;

  auto writer_task = [&]() -> boost::cobalt::task<void> {
    auto lk = co_await partition.write_lock();
    partition.push(message_t::create_message("first"));
    partition.push(message_t::create_message("second"));
    co_return;
  };

  auto reader_task = [&]() -> boost::cobalt::task<void> {
    auto lk = co_await partition.read_lock();
    EXPECT_EQ("first", partition.front().get_data());
    EXPECT_EQ("second", partition.back().get_data());
    co_return;
  };

  boost::cobalt::run(writer_task());
  boost::cobalt::run(reader_task());
}

} // namespace noctua
