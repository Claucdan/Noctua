#include "protocol-utils.h"

#include <cstddef>
#include <cstring>

namespace {

QByteArray create_request(noctua::rpc::opcode_t opcode,
                          const QByteArray& topic_name,
                          uint16_t partition_id,
                          const QByteArray& message) {
  noctua::rpc::request_header_t header{};
  header.magic = noctua::rpc::RPC_MAGIC;
  header.opcode = opcode;
  header.topic_name_len = static_cast<uint16_t>(topic_name.size());
  header.partition_id = partition_id;
  header.message_len = static_cast<uint16_t>(message.size());

  QByteArray packet;
  packet.resize(static_cast<int>(sizeof(header) + topic_name.size() + message.size()));

  auto* out = reinterpret_cast<std::byte*>(packet.data());
  std::memcpy(out, &header, sizeof(header));
  std::memcpy(out + sizeof(header), topic_name.constData(), static_cast<size_t>(topic_name.size()));
  if (!message.isEmpty()) {
    std::memcpy(out + sizeof(header) + topic_name.size(), message.constData(), static_cast<size_t>(message.size()));
  }

  return packet;
}

} // namespace

QByteArray protocol_utils_t::create_push_request(const QByteArray& topic_name,
                                                 uint16_t partition_id,
                                                 const QByteArray& message) {
  return create_request(opcode_t::PUSH, topic_name, partition_id, message);
}

QByteArray protocol_utils_t::create_pull_request(const QByteArray& topic_name, uint16_t partition_id) {
  return create_request(opcode_t::PULL, topic_name, partition_id, {});
}

QByteArray protocol_utils_t::create_delete_request(const QByteArray& topic_name,
                                                   uint16_t partition_id,
                                                   const QByteArray& message) {
  return create_request(opcode_t::DELETE, topic_name, partition_id, message);
}

protocol_utils_t::response_t protocol_utils_t::parse_response(const QByteArray& data) {
  response_t response;

  if (data.size() < static_cast<int>(sizeof(noctua::rpc::response_header_t))) {
    return response;
  }

  noctua::rpc::response_header_t header{};
  std::memcpy(&header, data.constData(), sizeof(header));

  if (header.magic != noctua::rpc::RPC_MAGIC) {
    return response;
  }

  const auto totalSize = sizeof(header) + static_cast<size_t>(header.message_len);
  if (data.size() < static_cast<int>(totalSize)) {
    return response;
  }

  response.valid = true;
  response.opcode = header.opcode;
  response.error_code = header.error_code;
  if (header.message_len > 0) {
    response.message = data.mid(static_cast<int>(sizeof(header)), header.message_len);
  }

  return response;
}
