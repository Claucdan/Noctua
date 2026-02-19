#include "src/partitions/partitions-storage.h"
#include "src/common/message.h"

#include <boost/cobalt.hpp>

#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace {

using message_t = noctua::common::message_t;
using message_ptr_t = message_t::message_ptr_t;

} // namespace

namespace noctua::partitions {

TEST(PartitionsStorageTest, ConstructorWithCount) {
  partititons_storage_t storage(5);
  EXPECT_EQ(5, storage.size());
}

TEST(PartitionsStorageTest, ConstructorWithZero) {
  partititons_storage_t storage(0);
  EXPECT_EQ(0, storage.size());
}

TEST(PartitionsStorageTest, ConstructorWithLargeCount) {
  partititons_storage_t storage(1000);
  EXPECT_EQ(1000, storage.size());
}

TEST(PartitionsStorageTest, GetPartitionByIndex) {
  partititons_storage_t storage(3);

  storage.get(0).push(message_t::create_message("first"));
  storage.get(1).push(message_t::create_message("second"));
  storage.get(2).push(message_t::create_message("third"));

  EXPECT_EQ("first", storage.get(0).front().get_data());
  EXPECT_EQ("second", storage.get(1).front().get_data());
  EXPECT_EQ("third", storage.get(2).front().get_data());
}

TEST(PartitionsStorageTest, GetPartitionByIndexMultipleMessages) {
  partititons_storage_t storage(2);

  storage.get(0).push(message_t::create_message("msg1"));
  storage.get(0).push(message_t::create_message("msg2"));
  storage.get(1).push(message_t::create_message("msg3"));

  EXPECT_EQ("msg1", storage.get(0).front().get_data());
  EXPECT_EQ("msg2", storage.get(0).back().get_data());
  EXPECT_EQ("msg3", storage.get(1).front().get_data());
}

TEST(PartitionsStorageTest, GetPartitionByHashString) {
  partititons_storage_t storage(3);

  storage.get("key1").push(message_t::create_message("value1"));
  storage.get("key2").push(message_t::create_message("value2"));
  storage.get("key3").push(message_t::create_message("value3"));

  EXPECT_EQ("value1", storage.get("key1").front().get_data());
  EXPECT_EQ("value2", storage.get("key2").front().get_data());
  EXPECT_EQ("value3", storage.get("key3").front().get_data());
}

TEST(PartitionsStorageTest, GetPartitionByHashInt) {
  partititons_storage_t storage(5);

  storage.get(10).push(message_t::create_message("ten"));
  storage.get(20).push(message_t::create_message("twenty"));
  storage.get(30).push(message_t::create_message("thirty"));

  EXPECT_EQ("ten", storage.get(10).front().get_data());
  EXPECT_EQ("twenty", storage.get(20).front().get_data());
  EXPECT_EQ("thirty", storage.get(30).front().get_data());
}

TEST(PartitionsStorageTest, HashConsistency) {
  partititons_storage_t storage(10);

  std::string key = "consistent_key";

  storage.get(key).push(message_t::create_message("value"));

  EXPECT_EQ("value", storage.get(key).front().get_data());
  storage.get(key).pop();
  EXPECT_TRUE(storage.get(key).messages_.empty());
}

TEST(PartitionsStorageTest, MultipleKeysSamePartition) {
  partititons_storage_t storage(1);

  storage.get("key1").push(message_t::create_message("first"));
  storage.get("key2").push(message_t::create_message("second"));

  EXPECT_EQ("first", storage.get(0).front().get_data());
  EXPECT_EQ("second", storage.get(0).back().get_data());
}

TEST(PartitionsStorageTest, MixedIndexAndHashAccess) {
  partititons_storage_t storage(5);

  storage.get(0).push(message_t::create_message("by_index"));
  storage.get("some_key").push(message_t::create_message("by_hash"));

  EXPECT_EQ("by_index", storage.get(0).front().get_data());
}

TEST(PartitionsStorageTest, PopFromPartition) {
  partititons_storage_t storage(2);

  storage.get(0).push(message_t::create_message("first"));
  storage.get(0).push(message_t::create_message("second"));

  EXPECT_EQ("first", storage.get(0).front().get_data());

  storage.get(0).pop();

  EXPECT_EQ("second", storage.get(0).front().get_data());
}

TEST(PartitionsStorageTest, CoroutineWithReadLock) {
  partititons_storage_t storage(2);

  storage.get(0).push(message_t::create_message("data"));

  auto task = [&]() -> boost::cobalt::task<void> {
    auto lk = co_await storage.get(0).read_lock();
    EXPECT_EQ("data", storage.get(0).front().get_data());
    co_return;
  };

  boost::cobalt::run(task());
}

TEST(PartitionsStorageTest, CoroutineWithWriteLock) {
  partititons_storage_t storage(2);

  auto task = [&]() -> boost::cobalt::task<void> {
    auto lk = co_await storage.get(1).write_lock();
    storage.get(1).push(message_t::create_message("written"));
    EXPECT_EQ("written", storage.get(1).front().get_data());
    co_return;
  };

  boost::cobalt::run(task());
}

} // namespace noctua::partitions
