#include "src/partitions/partition.h"
#include "src/common/message.h"

#include <boost/cobalt.hpp>
#include <chrono>

#include <gtest/gtest.h>

namespace {

using message_t = noctua::common::message_t;
using message_ptr_t = message_t::message_ptr_t;

} // namespace

namespace noctua::partitions {

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

} // namespace noctua::partitions
