#pragma once

#include "rpc/rpc-protocol.h"

#include "common/fibers/task.h"
#include "common/kassert.h"
#include "common/constants.h"
#include "common/fibers/sync-primitives/unique-mutex.h"

#include "topic.h"

#include <boost/asio/ip/tcp.hpp>
#include <fmt/core.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace noctua::rpc {

struct request_hash_t {
  std::size_t operator()(const common::request_t& request) const noexcept {
    std::size_t h = 0;
    std::string_view topic_name = request.topic_name();
    for (char c : topic_name) {
      h ^= std::hash<char>{}(c);
    }
    return h;
  }
};

using topic_type_t = topic_t<request_hash_t>;

class topic_registry_t {
public:
  topic_type_t* get_or_create_topic(std::string_view topic_name, size_t partitions_count = 1) {
    std::string key{topic_name};

    auto lock = mutex_.lock();
    auto it = topics_.find(key);
    if (it == topics_.end()) {
      auto [new_it, _] = topics_.emplace(std::move(key), std::make_unique<topic_type_t>(partitions_count, topic_name));
      return new_it->second.get();
    }
    if (it->second->partitions_count() < partitions_count) {
      it->second = std::make_unique<topic_type_t>(partitions_count, topic_name);
    }
    return it->second.get();
  }

  topic_type_t* get_topic(std::string_view topic_name) {
    std::string key{topic_name};

    auto lock = mutex_.lock();
    auto it = topics_.find(key);
    if (it == topics_.end()) {
      return nullptr;
    }
    return it->second.get();
  }

private:
  fibers::unique_mutex_t mutex_;
  std::unordered_map<std::string, std::unique_ptr<topic_type_t>> topics_;
};

class rpc_session_t : public std::enable_shared_from_this<rpc_session_t> {
public:
  using tcp_socket = boost::asio::ip::tcp::socket;

  rpc_session_t(topic_registry_t& registry, tcp_socket socket)
      : registry_(registry), socket_(std::move(socket)) {}

  fibers::task_t<void> start() {
    try {
      co_await handle_requests();
    } catch (const boost::system::system_error& e) {
      fmt::println(stderr, "[SESSION][{}] Connection error: {}", fmt::ptr(this), e.what());
    } catch (const std::exception& e) {
      fmt::println(stderr, "[SESSION][{}] Error: {}", fmt::ptr(this), e.what());
    } catch (...) {
      fmt::println(stderr, "[SESSION][{}] Unknown fatal error", fmt::ptr(this));
    }
  }

private:
  fibers::task_t<void> handle_requests() {
    std::array<std::byte, 65536> buffer;

    while (true) {
      request_header_t request_header;

      auto bytes_read = co_await boost::asio::async_read(
          socket_,
          boost::asio::buffer(&request_header, sizeof(request_header)),
          boost::asio::use_awaitable);

      if (bytes_read == 0) {
        break;
      }

      if (request_header.magic != RPC_MAGIC) {
        fmt::println(stderr, "[SESSION][{}] Invalid magic number in request header", fmt::ptr(this));
        co_await send_error_response(opcode_t::PUSH, error_code_t::INTERNAL_ERROR, "Invalid magic number");
        break;
      }

      size_t total_data_len = request_header.topic_name_len + request_header.message_len;
      if (total_data_len > buffer.size() - sizeof(request_header)) {
        fmt::println(stderr, "[SESSION][{}] Request data too large", fmt::ptr(this));
        co_await send_error_response(request_header.opcode, error_code_t::INTERNAL_ERROR, "Data too large");
        break;
      }

      if (total_data_len > 0) {
        bytes_read = co_await boost::asio::async_read(
            socket_,
            boost::asio::buffer(buffer.data(), total_data_len),
            boost::asio::use_awaitable);

        if (bytes_read == 0) {
          break;
        }
      }

      std::string_view topic_name{
          reinterpret_cast<const char*>(buffer.data()),
          request_header.topic_name_len};
      std::string_view message{
          reinterpret_cast<const char*>(buffer.data() + request_header.topic_name_len),
          request_header.message_len};

      co_await handle_request(request_header, topic_name, message);
    }

    fmt::println("[SESSION][{}] Connection closed", fmt::ptr(this));
  }

  fibers::task_t<void> handle_request(
      const request_header_t& header,
      std::string_view topic_name,
      std::string_view message) {

    switch (header.opcode) {
      case opcode_t::PUSH:
        co_await handle_push(header, topic_name, message);
        break;
      case opcode_t::PULL:
        co_await handle_pull(header, topic_name);
        break;
      case opcode_t::DELETE:
        co_await handle_delete(header, topic_name, message);
        break;
      default:
        co_await send_error_response(header.opcode, error_code_t::INTERNAL_ERROR, "Unknown opcode");
        break;
    }
  }

  fibers::task_t<void> handle_push(
      const request_header_t& header,
      std::string_view topic_name,
      std::string_view message) {
    if (topic_name.empty()) {
      co_await send_error_response(opcode_t::PUSH, error_code_t::INVALID_TOPIC_NAME, "Empty topic name");
      co_return;
    }

    auto* topic = registry_.get_or_create_topic(topic_name, std::max<size_t>(1, header.partition_id + 1));
    if (header.partition_id != common::INVALID_TOPIC_ID &&
        header.partition_id >= topic->partitions_count()) {
      co_await send_error_response(opcode_t::PUSH, error_code_t::PARTITION_NOT_FOUND, "Invalid partition ID");
      co_return;
    }

    std::vector<std::byte> request_buffer(required_request_buffer_size(topic_name, message));
    auto* request = create_request(request_buffer, topic_name, header.partition_id, message);
    co_await topic->push(*request);

    co_await send_ok_response(opcode_t::PUSH);
  }

  fibers::task_t<void> handle_pull(
      const request_header_t& header,
      std::string_view topic_name) {

    if (topic_name.empty()) {
      co_await send_error_response(opcode_t::PULL, error_code_t::INVALID_TOPIC_NAME, "Empty topic name");
      co_return;
    }

    auto* topic = registry_.get_topic(topic_name);
    if (topic == nullptr) {
      co_await send_error_response(opcode_t::PULL, error_code_t::TOPIC_NOT_FOUND, "Topic not found");
      co_return;
    }

    if (header.partition_id == common::INVALID_TOPIC_ID) {
      co_await send_error_response(opcode_t::PULL, error_code_t::INVALID_PARTITION_ID, "Partition ID required for PULL");
      co_return;
    }

    if (header.partition_id >= topic->partitions_count()) {
      co_await send_error_response(opcode_t::PULL, error_code_t::PARTITION_NOT_FOUND, "Invalid partition ID");
      co_return;
    }

    auto result = co_await topic->read(header.partition_id);
    if (!result.has_value()) {
      co_await send_error_response(opcode_t::PULL, error_code_t::EMPTY_PARTITION, "No messages in partition");
      co_return;
    }

    co_await send_data_response(opcode_t::PULL, *result);
  }

  fibers::task_t<void> handle_delete(
      const request_header_t& header,
      std::string_view topic_name,
      std::string_view message) {

    if (topic_name.empty()) {
      co_await send_error_response(opcode_t::DELETE, error_code_t::INVALID_TOPIC_NAME, "Empty topic name");
      co_return;
    }

    auto* topic = registry_.get_topic(topic_name);
    if (topic == nullptr) {
      co_await send_error_response(opcode_t::DELETE, error_code_t::TOPIC_NOT_FOUND, "Topic not found");
      co_return;
    }

    if (header.partition_id == common::INVALID_TOPIC_ID) {
      co_await send_error_response(opcode_t::DELETE, error_code_t::INVALID_PARTITION_ID, "Partition ID required for DELETE");
      co_return;
    }

    if (header.partition_id >= topic->partitions_count()) {
      co_await send_error_response(opcode_t::DELETE, error_code_t::PARTITION_NOT_FOUND, "Invalid partition ID");
      co_return;
    }

    std::vector<std::byte> request_buffer(required_request_buffer_size(topic_name, message));
    auto* request = create_request(request_buffer, topic_name, header.partition_id, message);
    co_await topic->remove(*request);

    co_await send_ok_response(opcode_t::DELETE);
  }

  fibers::task_t<void> send_ok_response(opcode_t opcode) {
    response_header_t response{};
    response.opcode = opcode;
    response.error_code = error_code_t::OK;
    co_await boost::asio::async_write(
        socket_,
        boost::asio::buffer(&response, sizeof(response)),
        boost::asio::use_awaitable);
  }

  fibers::task_t<void> send_data_response(opcode_t opcode, std::string_view data) {
    response_header_t response{};
    response.opcode = opcode;
    response.error_code = error_code_t::OK;
    response.message_len = static_cast<uint16_t>(data.size());

    std::array<boost::asio::const_buffer, 2> buffers{
        boost::asio::buffer(&response, sizeof(response)),
        boost::asio::buffer(data.data(), data.size())};
    co_await boost::asio::async_write(socket_, buffers, boost::asio::use_awaitable);
  }

  fibers::task_t<void> send_error_response(opcode_t opcode, error_code_t error_code, std::string_view error_message) {
    response_header_t response{};
    response.opcode = opcode;
    response.error_code = error_code;
    response.message_len = static_cast<uint16_t>(error_message.size());

    std::array<boost::asio::const_buffer, 2> buffers{
        boost::asio::buffer(&response, sizeof(response)),
        boost::asio::buffer(error_message.data(), error_message.size())};
    co_await boost::asio::async_write(socket_, buffers, boost::asio::use_awaitable);
  }

  [[nodiscard]] size_t required_request_buffer_size(
      std::string_view topic_name,
      std::string_view message) const noexcept {
    return sizeof(uint16_t) * 3 + topic_name.size() + message.size();
  }

  const common::request_t* create_request(
      std::span<std::byte> buffer,
      std::string_view topic_name,
      uint16_t partition_id,
      std::string_view message) {

    const auto required_size = required_request_buffer_size(topic_name, message);
    if (buffer.size() < required_size) {
      throw std::runtime_error("Request buffer too small");
    }

    auto offset = buffer.data();

    uint16_t topic_name_len = static_cast<uint16_t>(topic_name.size());
    std::memcpy(offset, &topic_name_len, sizeof(topic_name_len));
    offset += sizeof(topic_name_len);

    std::memcpy(offset, &partition_id, sizeof(partition_id));
    offset += sizeof(partition_id);

    uint16_t message_len = static_cast<uint16_t>(message.size());
    std::memcpy(offset, &message_len, sizeof(message_len));
    offset += sizeof(message_len);

    std::memcpy(offset, topic_name.data(), topic_name.size());
    offset += topic_name.size();

    std::memcpy(offset, message.data(), message.size());

    return reinterpret_cast<const common::request_t*>(buffer.data());
  }

private:
  topic_registry_t& registry_;
  tcp_socket socket_;
};

} // namespace noctua::rpc
