#pragma once

#include "rpc/rpc-protocol.h"

#include <boost/asio.hpp>

#include <fmt/core.h>

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace noctua::rpc {

class rpc_client_t {
public:
  rpc_client_t()
      : resolver_(io_ctx_), socket_(io_ctx_) {}

  void connect(std::string_view host, uint16_t port) {
    boost::asio::ip::tcp::resolver::results_type endpoints =
        resolver_.resolve(host, std::to_string(port));

    socket_.connect(*endpoints.begin());
    connected_ = true;
  }

  void disconnect() {
    if (connected_) {
      boost::system::error_code ec;
      socket_.close(ec);
      connected_ = false;
    }
  }

  bool is_connected() const noexcept {
    return connected_;
  }

  void send_push(std::string_view topic_name, uint16_t partition_id, std::string_view message) {
    send_request(opcode_t::PUSH, topic_name, partition_id, message);
  }

  void send_pull(std::string_view topic_name, uint16_t partition_id) {
    send_request(opcode_t::PULL, topic_name, partition_id, "");
  }

  void send_delete(std::string_view topic_name, uint16_t partition_id, std::string_view message) {
    send_request(opcode_t::DELETE, topic_name, partition_id, message);
  }

  response_header_t receive_response() {
    response_header_t response;
    boost::asio::read(socket_, boost::asio::buffer(&response, sizeof(response)));
    return response;
  }

  std::optional<std::string> receive_response_data(const response_header_t& response) {
    if (response.message_len == 0) {
      return std::nullopt;
    }

    std::vector<char> buffer(response.message_len);
    boost::asio::read(socket_, boost::asio::buffer(buffer.data(), buffer.size()));
    return std::string(buffer.begin(), buffer.end());
  }

  ~rpc_client_t() {
    disconnect();
  }

private:
  void send_request(opcode_t opcode, std::string_view topic_name, uint16_t partition_id, std::string_view message) {
    std::vector<std::byte> buffer;

    request_header_t header{};
    header.magic = RPC_MAGIC;
    header.opcode = opcode;
    header.topic_name_len = static_cast<uint16_t>(topic_name.size());
    header.partition_id = partition_id;
    header.message_len = static_cast<uint16_t>(message.size());

    size_t total_size = sizeof(header) + header.topic_name_len + header.message_len;
    buffer.resize(total_size);

    size_t offset = 0;
    std::memcpy(buffer.data() + offset, &header, sizeof(header));
    offset += sizeof(header);
    std::memcpy(buffer.data() + offset, topic_name.data(), topic_name.size());
    offset += topic_name.size();
    std::memcpy(buffer.data() + offset, message.data(), message.size());

    boost::asio::write(socket_, boost::asio::buffer(buffer));
  }

  boost::asio::io_context io_ctx_;
  boost::asio::ip::tcp::resolver resolver_;
  boost::asio::ip::tcp::socket socket_;
  bool connected_{false};
};

} // namespace noctua::rpc
